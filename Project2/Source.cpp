#include <Windows.h>
#include <vector>
#include <string>
#include <iostream>
#include <random>
#include <utility>
#include <algorithm>
#include <fstream>
#include <math.h>
#include <chrono>

#include <opencv2/opencv.hpp>
#include <opencv2/text/ocr.hpp>

using namespace cv;
//using namespace dnn;
using namespace std;
using namespace cv::text;

HWND window;

int height, width;
int msdThreshold = 10000;
int loadTime = 10000;
double heightPct, widthPct;
bool useAbsFishLoc = false;
WORD xCenter, yCenter;
Ptr<OCRTesseract> ocr;
HDC hdc;
HDC hDest;
HBITMAP hbDesktop;
Mat bitbltPic, fishIcon, exclamationIcon, swampFishIcon;
std::random_device dev;
std::mt19937 rng;
std::uniform_int_distribution<std::mt19937::result_type> lClickRand;
std::uniform_int_distribution<std::mt19937::result_type> sleepRand;
std::uniform_int_distribution<std::mt19937::result_type> slideSleepRand;
std::uniform_int_distribution<std::mt19937::result_type> slideRand;
std::uniform_int_distribution<std::mt19937::result_type> slideDistanceRand;
std::uniform_int_distribution<std::mt19937::result_type> boolRand;
std::uniform_int_distribution<std::mt19937::result_type> slideLClick;
std::uniform_int_distribution<std::mt19937::result_type> longSleepRand;

string ocrNumbers = "/1234567890";
string ocrLetters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.'";

pair<int, int> attackButton = { 1595, 845 };
pair<int, int> mapButton = { 380, 900 };
pair<int, int> antiquity = { 116, 148 };
pair<int, int> present = { 279, 153 };
pair<int, int> futur = { 444, 151 };
pair<int, int> endOfTimeLoc = { 610, 137 };
pair<int, int> yesButton = { 1068, 559 };
pair<int, int> leaveButton = { 1560, 922 };
pair<int, int> xButton = { 1699, 32 };
vector<pair<int, int>> acteulLocs = { {900, 506}, {675, 377}, {1131, 392}, {657, 643}, {1130, 648}, };
vector<pair<int, int>> baruokiLocs = { {842, 508}, {632, 409}, {1047, 402}, {630, 634}, {1046, 623}, };
vector<pair<int, int>> dragonPalaceLocs = { {870, 483}, {654, 402}, {1055, 399}, {673, 580}, {1069, 596}, };
vector<pair<int, int>> elzionLocs = { {909, 517}, {691, 397}, {1154, 429}, {673, 643}, {1166, 659}, };
vector<pair<int, int>> dimensionRiftLocs = { {887, 481}, {604, 470}, {1112, 471} };
vector<pair<int, int>> kiraBeachLocs = { {936, 533}, {697, 389}, {1180, 404}, {705, 686}, {1173, 687} };
vector<pair<int, int>> rucyanaSandsLocs = { {862, 494}, {650, 398}, {1080, 403}, {632, 603}, {1103, 622} };
vector<pair<int, int>> vasuLocs = { {879, 492}, {626, 395}, {1133, 399}, {612, 598}, {1166, 614} };

enum baitType { Fishing_Dango, Worm, Unexpected_Worm, Shopaholic_Clam, Spree_Snail, Dressed_Crab, Tear_Crab, Foamy_Worm, Bubbly_Worm, Snitch_Sardines, Babbling_Sardines, Crab_Cake, Premium_Crab_Cake };

struct fishingSpot
{
	fishingSpot(void(*fPtr)(), int orderNum) : fishFunction(fPtr), orderNumber(orderNum) {}
	vector<baitType> baitsToUse;
	void(*fishFunction)();
	int orderNumber;
};

vector<pair<bool, int>> baitList; //Index is the type, the boolean is whether or not you have greater than 0 currently held, int is how many to buy for this run

vector<fishingSpot> fishingSpots;
pair<int, int> currentFishIconLoc;
vector<baitType>* currentBaitsToUse;
int currentHorrorBaitThreshold = 9999;

BOOL CALLBACK EnumWindowsProc(_In_ HWND hwnd, _In_ LPARAM lParam)
{
	DWORD dSize = MAX_PATH;
	TCHAR buffer[MAX_PATH] = { 0 };
	DWORD dwProcId = 0;

	GetWindowThreadProcessId(hwnd, &dwProcId);

	HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcId);
	QueryFullProcessImageName(hProc, 0, buffer, &dSize);
	CloseHandle(hProc);

	string windowTitle(buffer);

	string finalWindow = windowTitle.substr(windowTitle.rfind("\\") + 1, string::npos);

	if (finalWindow.compare((*((pair<string*, string*>*)(lParam))->first)) == 0)
	{
		GetWindowTextA(hwnd, buffer, MAX_PATH);
		windowTitle = buffer;

		if (windowTitle.compare((*((pair<string*, string*>*)(lParam))->second)) == 0)
		{
			window = hwnd;
			return false;
		}
	}

	return true;
}

BOOL CALLBACK EnumChildWindowsProc(_In_ HWND hwnd, _In_ LPARAM lParam)
{
	TCHAR buffer[MAX_PATH] = { 0 };
	GetWindowTextA(hwnd, buffer, MAX_PATH);
	string windowTitle = buffer;

	if (windowTitle.compare((*((string*)(lParam)))) == 0)
	{
		window = hwnd;
		return false;
	}

	return true;
}

void sleepR(int time)
{
	Sleep(time + sleepRand(rng));
}

void longSleepR(int time)
{
	Sleep(time + longSleepRand(rng));
}

void rSlideSleep(int time)
{
	Sleep(time + slideSleepRand(rng));
}

enum Direction
{
	LEFT, RIGHT, DOWN, UP
};

void bitBltWholeScreen()
{
	BitBlt(hDest, 0, 0, width, height, hdc, 0, 0, SRCCOPY);
}

void copyPartialPic(Mat& partialPic, int cols, int rows, int x, int y)
{
	x = round(x * widthPct);
	y = round(y * heightPct);
	cols = round(cols * widthPct);
	rows = round(rows * heightPct);

	bitbltPic(cv::Rect(x, y, cols, rows)).copyTo(partialPic);
}

string getText(Mat& pic)
{
	vector<Rect>   boxes;
	vector<string> words;
	vector<float>  confidences;
	string output;
	Mat newPic;
	cvtColor(pic, newPic, COLOR_BGRA2BGR);
	ocr->run(newPic, output, &boxes, &words, &confidences, OCR_LEVEL_TEXTLINE);
	if (words.empty())
		return "";
	return words[0].substr(0, words[0].size() - 1);
}

pair<int, int> findIcon(Mat& tmp)
{
	bitBltWholeScreen();
	int result_cols = bitbltPic.cols - tmp.cols + 1;
	int result_rows = bitbltPic.rows - tmp.rows + 1;
	Mat result;
	result.create(result_rows, result_cols, CV_32FC1);

	matchTemplate(bitbltPic, tmp, result, TM_SQDIFF_NORMED);

	Point minLoc, maxLoc;
	double minVal, maxVal;

	minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc, Mat());

	return make_pair(minLoc.x, minLoc.y);
}

pair<int, int> findFishIcon()
{
	pair<int, int> fishIconLoc = findIcon(fishIcon);
	fishIconLoc.first += round(30 * widthPct);
	fishIconLoc.second += round(15 * heightPct);
	return fishIconLoc;
}

pair<int, int> findSwampFishIcon()
{
	pair<int, int> fishIconLoc = findIcon(swampFishIcon);
	fishIconLoc.first += round(36 * widthPct);
	fishIconLoc.second += round(19 * heightPct);
	return fishIconLoc;
}

pair<int, int> findExclamationIcon()
{
	pair<int, int> exclIcon = findIcon(exclamationIcon);
	exclIcon.first += round(11 * widthPct);
	exclIcon.second += round(25 * heightPct);
	return exclIcon;
}

int getNumber(Mat& pic)
{
	return stoi(getText(pic));
}

void leftClick(int x, int y, int sTime = 2000, bool changeLoc = true)
{
	if (changeLoc)
	{
		x = round(x * widthPct);
		y = round(y * heightPct);
	}

	int randX = (boolRand(dev) ? lClickRand(rng) : ((-1)*lClickRand(rng)));
	int randY = (boolRand(dev) ? lClickRand(rng) : ((-1)*lClickRand(rng)));
	SendMessage(window, WM_LBUTTONDOWN, 0, MAKELPARAM(x + randX, y + randY));
	sleepR(10);
	SendMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(x + randX, y + randY));

	if (sTime <= 100)
		Sleep(sTime + lClickRand(rng));
	else
		Sleep(sTime + slideLClick(rng));
}

void leftClick(pair<int, int>& coord, int sTime = 2000)
{
	leftClick(coord.first, coord.second, sTime);
}

void drag(Direction direction, int slideDistance, int xStart, int yStart)
{
	xStart = round(xStart * widthPct);
	yStart = round(yStart * heightPct);
	int delta = 0;
	int deviation = 3;

	switch (direction)
	{
	case RIGHT:
		deviation = round(deviation*heightPct);
		slideDistance = round(slideDistance * widthPct);
		xStart = (boolRand(rng) ? xStart + slideLClick(rng) : xStart - slideLClick(rng));
		yStart = (boolRand(rng) ? yStart + slideLClick(rng) : yStart - slideLClick(rng));
		SendMessage(window, WM_LBUTTONDOWN, 0, MAKELPARAM(xStart, yStart));
		sleepR(10);

		for (int i = 0; i < slideDistance; ++i)
		{
			if (boolRand(rng) && delta < deviation)
				delta += boolRand(rng);
			else if (delta > -deviation)
				delta -= boolRand(rng);

			SendMessage(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(xStart - i, yStart + delta));
			rSlideSleep(1);
		}

		SendMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(xStart - (slideDistance - 1), yStart + delta));
		break;
	case LEFT:
		deviation = round(deviation*heightPct);
		slideDistance = round(slideDistance * widthPct);
		xStart = (boolRand(rng) ? xStart + slideLClick(rng) : xStart - slideLClick(rng));
		yStart = (boolRand(rng) ? yStart + slideLClick(rng) : yStart - slideLClick(rng));
		SendMessage(window, WM_LBUTTONDOWN, 0, MAKELPARAM(xStart, yStart));
		sleepR(10);

		for (int i = 0; i < slideDistance; ++i)
		{
			if (boolRand(rng) && delta < deviation)
				delta += boolRand(rng);
			else if (delta > -deviation)
				delta -= boolRand(rng);

			SendMessage(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(xStart + i, yStart + delta));
			rSlideSleep(1);
		}

		SendMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(xStart + (slideDistance - 1), yStart + delta));
		break;
	case UP:
		deviation = round(deviation*widthPct);
		slideDistance = round(slideDistance * heightPct);
		xStart = (boolRand(rng) ? xStart + slideLClick(rng) : xStart - slideLClick(rng));
		yStart = (boolRand(rng) ? yStart + slideLClick(rng) : yStart - slideLClick(rng));
		SendMessage(window, WM_LBUTTONDOWN, 0, MAKELPARAM(xStart, yStart));
		sleepR(10);

		for (int i = 0; i < slideDistance; ++i)
		{
			if (boolRand(rng) && delta < deviation)
				delta += boolRand(rng);
			else if (delta > -deviation)
				delta -= boolRand(rng);

			SendMessage(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(xStart + delta, yStart + i));
			rSlideSleep(1);
		}

		SendMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(xStart + delta, yStart + (slideDistance - 1)));
		break;
	case DOWN:
		deviation = round(deviation*widthPct);
		slideDistance = round(slideDistance * heightPct);
		xStart = (boolRand(rng) ? xStart + slideLClick(rng) : xStart - slideLClick(rng));
		yStart = (boolRand(rng) ? yStart + slideLClick(rng) : yStart - slideLClick(rng));
		SendMessage(window, WM_LBUTTONDOWN, 0, MAKELPARAM(xStart, yStart));
		sleepR(10);

		for (int i = 0; i < slideDistance; ++i)
		{
			if (boolRand(rng) && delta < deviation)
				delta += boolRand(rng);
			else if (delta > -deviation)
				delta -= boolRand(rng);

			SendMessage(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(xStart + delta, yStart - i));
			rSlideSleep(1);
		}

		SendMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(xStart + delta, yStart - (slideDistance - 1)));
		break;
	}

	longSleepR(1000);
}

void dragMap(Direction direction, int slideDistance)
{
	switch (direction)
	{
	case RIGHT:
		drag(RIGHT, slideDistance, 1600, 600);
		break;
	case LEFT:
		drag(LEFT, slideDistance, 185, 600);
		break;
	case UP:
		drag(UP, slideDistance, 850, 185);
		break;
	case DOWN:
		drag(DOWN, slideDistance, 850, 850);
		break;
	}
}

void Walk(Direction direction, int time, bool noRngSleep = false)
{
	int delta = 0;
	int slideDistance;
	int deviation = 3;
	int xStart = (boolRand(rng) ? (round(xCenter*widthPct)) + slideLClick(rng) : (round(xCenter*widthPct)) - slideLClick(rng));
	int yStart = (boolRand(rng) ? (round(yCenter*heightPct)) + slideLClick(rng) : (round(yCenter*heightPct)) - slideLClick(rng));

	//Start walking left
	SendMessage(window, WM_LBUTTONDOWN, 0, MAKELPARAM(xStart, yStart));
	Sleep(10);

	switch (direction)
	{
	case LEFT:
		deviation = round(deviation*heightPct);
		slideDistance = (boolRand(rng) ? slideDistanceRand(rng) : (-1)*slideDistanceRand(rng)) + 300;
		slideDistance = round(slideDistance * widthPct);
		for (int i = 0; i < slideDistance; ++i)
		{
			if (boolRand(rng) && delta < 3)
				delta += boolRand(rng);
			else if (delta > -3)
				delta -= boolRand(rng);

			SendMessage(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(xStart - i, yStart + delta));
			if (!noRngSleep)
				Sleep(1);
		}

		Sleep(time);
		SendMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(xStart - (slideDistance - 1), yStart + delta));
		break;

	case RIGHT:
		deviation = round(deviation*heightPct);
		slideDistance = (boolRand(rng) ? slideDistanceRand(rng) : (-1)*slideDistanceRand(rng)) + 300;
		slideDistance = round(slideDistance * widthPct);
		for (int i = 0; i < slideDistance; ++i)
		{
			if (boolRand(rng) && delta < 3)
				delta += boolRand(rng);
			else if (delta > -3)
				delta -= boolRand(rng);

			SendMessage(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(xStart + i, yStart + delta));
			if (!noRngSleep)
				Sleep(1);
		}

		Sleep(time);
		SendMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(xStart + (slideDistance - 1), yStart + delta));
		break;

	case DOWN:
		deviation = round(deviation*widthPct);
		slideDistance = (boolRand(rng) ? slideDistanceRand(rng) : (-1)*slideDistanceRand(rng)) + 300;
		slideDistance = round(slideDistance * heightPct);
		for (int i = 0; i < slideDistance; ++i)
		{
			if (boolRand(rng) && delta < 3)
				delta += boolRand(rng);
			else if (delta > -3)
				delta -= boolRand(rng);

			SendMessage(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(xStart + delta, yStart + i));
			if (!noRngSleep)
				Sleep(1);
		}

		Sleep(time);
		SendMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(xStart + delta, yStart + (slideDistance - 1)));
		break;

	case UP:
		deviation = round(deviation*widthPct);
		slideDistance = (boolRand(rng) ? slideDistanceRand(rng) : (-1)*slideDistanceRand(rng)) + 300;
		slideDistance = round(slideDistance * heightPct);
		for (int i = 0; i < slideDistance; ++i)
		{
			if (boolRand(rng) && delta < 3)
				delta += boolRand(rng);
			else if (delta > -3)
				delta -= boolRand(rng);

			SendMessage(window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(xStart + delta, yStart - i));
			if (!noRngSleep)
				Sleep(1);
		}

		Sleep(time);
		SendMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(xStart + delta, yStart - (slideDistance - 1)));
		break;

	}

	longSleepR(1000);
}

void clickAttack(int time = 5000)
{
	leftClick(attackButton, time);
}

bool inBattle()
{
	Mat partialPic;
	bitBltWholeScreen();
	copyPartialPic(partialPic, 106, 39, 77, 37);
	return getText(partialPic).compare("Status") == 0;
}

void fightUntilEnd()
{
	do
	{
		clickAttack(7000);
	} while (inBattle());

	clickAttack(); //Get past the results screen
}

void WalkUntilBattle()
{
	while (!inBattle())
	{
		Walk(LEFT, 500);
		if (inBattle())
			break;

		Walk(RIGHT, 500);
	}
	fightUntilEnd();
}

string getStatus()
{
	ocr->setWhiteList(ocrLetters);
	bitBltWholeScreen();
	cv::Mat partialPic;
	copyPartialPic(partialPic, 1242, 71, 257, 80);
	return getText(partialPic);
}

//Strategy is to quadrisect the lake and toss into each of the four sections and center
void fish(vector<pair<int, int>>& sections, int msdThreshold = 10000)
{
	longSleepR(3000);
	vector<pair<int, int>> baits = { {827, 274}, {882, 397}, {870, 514}, {851, 630}, {854, 750}, {884, 790} };

	auto changeBait = [&baits](baitType type)
	{
		int slotNum = 0;
		if (type > 0)
		{
			//Calculate slot that bait is in
			for (int i = 0; i < type; ++i)
			{
				if (baitList[i].first == true)
					++slotNum;
			}
		}

		leftClick(1225, 925);

		if (slotNum > 4)
		{
			drag(DOWN, 74, xCenter, yCenter);
			for (int i = 5; i < slotNum; ++i)
			{
				drag(DOWN, 100, xCenter, yCenter);
			}

			leftClick(baits[5].first, baits[5].second);
		}
		else
			leftClick(baits[slotNum].first, baits[slotNum].second);
	};

	bitBltWholeScreen();
	Mat lakeImg = bitbltPic.clone();

	for (int i = 0; i < currentBaitsToUse->size(); ++i)
	{
		if (baitList[(*currentBaitsToUse)[i]].first == false) //If we have 0 of current bait, go to next bait
			continue;

		changeBait((*currentBaitsToUse)[i]);

		shuffle(sections.begin() + 1, sections.end(), default_random_engine(std::chrono::system_clock::now().time_since_epoch().count()));

		bool increaseSection = false;
		for (int j = 0; j < sections.size(); )
		{
			//Cast line
			leftClick(sections[j].first, sections[j].second);

			string status = getStatus();
			if (status.find("bait") != string::npos) //Out of bait
			{
				baitList[(*currentBaitsToUse)[i]].first = false;

				do //Look for a bait that we actually have
				{
					++i;
				} while (i < currentBaitsToUse->size() && baitList[(*currentBaitsToUse)[i]].first == false);

				if (i >= currentBaitsToUse->size())
					return;

				changeBait((*currentBaitsToUse)[i]);
				leftClick(sections[j].first, sections[j].second);
			}
			else if (status.find("any fish") != string::npos || status.find("box") != string::npos) //Pool is empty or cooler is full
				return;

			double MSD;
			auto startTime = chrono::high_resolution_clock::now();
			bool noFishCheck = true;

			while (1) //This constantly reads the screen looking for either a certain status, or whether a zoom in has occurred
			{
				//getStatus() is really cpu intensive, so avoid doing it unless necessary
				if (std::chrono::duration_cast<std::chrono::milliseconds>(chrono::high_resolution_clock::now() - startTime).count() >= 11000 + longSleepRand(rng) && noFishCheck)
				{
					noFishCheck = false;
					status = getStatus();

					if (status.find("no fish") != string::npos)
					{
						increaseSection = true;
						leftClick(sections[j].first, sections[j].second);

						break;
					}
				}

				bitBltWholeScreen();
				MSD = cv::norm(lakeImg, bitbltPic);
				MSD = MSD * MSD / lakeImg.total();

				if (MSD > msdThreshold) //If the current screen is sufficiently different (high mean square difference) from the normal lake image, then a zoom in has occurred
				{
					Sleep(200 + lClickRand(rng)); //Emulate human reaction time
					leftClick(sections[j].first, sections[j].second);

					//Click through success or failure
					longSleepR(4000);
					leftClick(sections[j].first, sections[j].second);
					longSleepR(2000);

					bitBltWholeScreen();
					MSD = cv::norm(lakeImg, bitbltPic);
					MSD = MSD * MSD / lakeImg.total();
					if (MSD > msdThreshold) //Double, gotta click past it
					{
						leftClick(sections[j].first, sections[j].second);
						longSleepR(2000);

						bitBltWholeScreen();
						MSD = cv::norm(lakeImg, bitbltPic);
						MSD = MSD * MSD / lakeImg.total();
						if (MSD > msdThreshold) //Triple
						{
							leftClick(sections[j].first, sections[j].second);
							longSleepR(2000);
						}
					}

					//Check for battle
					bitBltWholeScreen();
					MSD = cv::norm(lakeImg, bitbltPic);
					MSD = MSD * MSD / lakeImg.total();
					if (MSD > msdThreshold) //Should have returned to normal lake image; if not, its a battle
					{
						if ((*currentBaitsToUse)[i] < currentHorrorBaitThreshold) //Not a horror or lake lord, so should be able to auto attack it down
						{
							fightUntilEnd();
							clickAttack(); //Click past fish results screen

							if (useAbsFishLoc)  //Hack for last island since it needs to use the template to find the fish icon, which means the points don't need to be scaled
								leftClick(currentFishIconLoc.first, currentFishIconLoc.second, 2000, false);
							else
								leftClick(currentFishIconLoc.first, currentFishIconLoc.second);

							longSleepR(4000);
						}
						else //Its a horror or lakelord, so need to pause and let the user fight manually
						{
							cout << "Horror or Lake Lord detected. Finish the fight manually, re-enter the fishing lake (click fish icon), and then press Enter to continue fishing . . . ";
							cin.get();
						}
					}

					break;
				}

				Sleep(12); //Try to stay around 60 fps read time
			}

			if (increaseSection)
				++j;

			increaseSection = false;
		}
	}

}

void goToSpacetimeRift()
{
	leftClick(mapButton, 3500);
	leftClick(endOfTimeLoc);
	leftClick(869, 482);
	leftClick(869, 482);
	leftClick(yesButton);
	longSleepR(loadTime);
}

void goToFishVendor()
{
	auto getVendorText = [](int columns, int rows, int x, int y)
	{
		bitBltWholeScreen();
		cv::Mat partialPic;
		ocr->setWhiteList(ocrLetters);
		copyPartialPic(partialPic, columns, rows, x, y);
		return getText(partialPic);
	};

	auto getVendorNumber = [](int columns, int rows, int x, int y)
	{
		bitBltWholeScreen();
		cv::Mat partialPic;
		ocr->setWhiteList(ocrNumbers);
		copyPartialPic(partialPic, columns, rows, x, y);
		return getNumber(partialPic);
	};

	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(antiquity);
	leftClick(538, 310);
	leftClick(yesButton);
	longSleepR(loadTime);
	Walk(LEFT, 800);
	longSleepR(1000);

	pair<int, int> pnt = findExclamationIcon();
	leftClick(pnt.first, pnt.second, 2000, false);

	for (int i = 0; i < 30; ++i) //click through text
		leftClick(380, 355, 100);

	leftClick(878, 888);

	//Buy baits if needed
	vector<pair<int, int>> baitsLocs = { {1220, 160}, {1215, 325}, {1215, 465}, {1215, 625}, {1215, 800}, {1225, 925} };
	vector<int> baitCosts = { 500, 1000, 3000, 1, 2, 3, 4, 6, 10, 30, 4000, 6000 };
	string currText;

	//Only 5 bait slots are fully on screen at a time, with the 6th partially visible. For every bait after 6, scroll down until new bait occupies bottom slot and then attempt to buy
	for (int i = 0; i < baitList.size(); ++i)
	{
		if (i > 4)
		{
			if (i == 5)
				drag(DOWN, 60, 1225, 550);
			else
				drag(DOWN, 135, 1225, 550);
			leftClick(baitsLocs[5].first, baitsLocs[5].second);
		}
		else
			leftClick(baitsLocs[i].first, baitsLocs[i].second);

		//If the next one we've done is the same as the last, we've reached the end of the list
		string txt = getVendorText(577, 51, 100, 102);
		if (currText.compare(txt) == 0)
			break;
		currText = txt;

		//Get amount currently held
		int held = getVendorNumber(141, 30, 500, 165);
		int numToBuy = baitList[i].second - held;

		if (numToBuy > 0)
		{
			if (i > 2 && i < 10) //Fish point bait. Here, since fish points aren't infinite like gold, only buy as much as you can up to the max set in config
			{
				//Get amount of fish points owned
				int fishPoints = getVendorNumber(193, 28, 1346, 10);

				//Get cost of current selected bait
				int fpCost = baitCosts[i];

				if (fpCost < fishPoints) //Can we even afford one?
				{
					if (numToBuy * fpCost >= fishPoints) //If we don't have enough fish points to buy the max set, then buy as many as we can
						numToBuy = floor(fishPoints / fpCost);
				}
			}

			if (numToBuy > 0)
			{
				for (int j = 0; j < numToBuy - 1; ++j)
					leftClick(555, 665, 500);

				leftClick(350, 850);
				leftClick(1070, 650);
				leftClick(880, 575);

				held = getVendorNumber(141, 30, 500, 165);
			}
		}

		baitList[i].first = held > 0 ? true : false;
	}

	leftClick(xButton);
}

void goToBaruoki()
{
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(present);
	dragMap(LEFT, 1700);
	dragMap(UP, 600);
	leftClick(576, 587);
	leftClick(yesButton);
	longSleepR(loadTime);
}

void kiraBeach()
{
	useAbsFishLoc = true;

	goToFishVendor();
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(antiquity);
	leftClick(538, 310);
	leftClick(yesButton);
	longSleepR(loadTime);

	Walk(LEFT, 1600);
	longSleepR(3000);

	currentFishIconLoc = findFishIcon();
	leftClick(currentFishIconLoc.first, currentFishIconLoc.second, 2000, false);

	longSleepR(1000);

	fish(kiraBeachLocs);
	leftClick(leaveButton);

	useAbsFishLoc = false;
}

void baruoki()
{
	currentHorrorBaitThreshold = Snitch_Sardines;

	goToFishVendor();
	goToBaruoki();
	Walk(LEFT, 500);
	Walk(UP, 100);
	Walk(LEFT, 10000);
	leftClick(711, 366);
	currentFishIconLoc = make_pair(711, 366);

	fish(baruokiLocs);
	leftClick(leaveButton);

	currentHorrorBaitThreshold = 9999;
}

void naaruUplands()
{
	goToFishVendor();
	goToBaruoki();
	Walk(LEFT, 10000);
	Walk(DOWN, 100);
	Walk(LEFT, 20000);
	fightUntilEnd();
	Walk(LEFT, 5000);
	leftClick(775, 450);
	currentFishIconLoc = make_pair(775, 450);

	fish(baruokiLocs);
	leftClick(leaveButton);
}

void goToActeul()
{
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(antiquity);
	leftClick(746, 741);
	leftClick(yesButton);
	longSleepR(loadTime);
}

void acteul()
{
	currentHorrorBaitThreshold = Snitch_Sardines;

	goToFishVendor();
	goToActeul();
	Walk(LEFT, 400);
	Walk(UP, 100);
	Walk(RIGHT, 10000);
	leftClick(1029, 367);
	currentFishIconLoc = make_pair(1029, 367);

	fish(acteulLocs);
	leftClick(leaveButton);

	currentHorrorBaitThreshold = 9999;
}

void elzionAirport()
{
	currentHorrorBaitThreshold = Snitch_Sardines;

	goToFishVendor();
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(futur);
	leftClick(751, 637);
	leftClick(yesButton);
	longSleepR(loadTime);
	Walk(RIGHT, 200);

	pair<int, int> exclIcon = findExclamationIcon();
	leftClick(exclIcon.first, exclIcon.second, 2000, false);
	leftClick(exclIcon.first, exclIcon.second, 2000, false);

	leftClick(884, 512);
	longSleepR(loadTime);
	Walk(RIGHT, 20000);
	WalkUntilBattle();
	Walk(RIGHT, 10000);
	leftClick(456, 312);
	longSleepR(loadTime);
	Walk(RIGHT, 5000);
	leftClick(1004, 448);
	currentFishIconLoc = make_pair(1004, 448);

	fish(elzionLocs);
	leftClick(leaveButton);

	currentHorrorBaitThreshold = 9999;
}

void lakeTillian()
{
	goToFishVendor();
	goToActeul();
	Walk(RIGHT, 10000);
	Walk(UP, 100);
	Walk(RIGHT, 10000);
	Walk(LEFT, 200);
	Walk(DOWN, 100);
	Walk(RIGHT, 10000);
	WalkUntilBattle();
	Walk(RIGHT, 5000);
	Walk(UP, 100);
	Walk(RIGHT, 10000);
	Walk(DOWN, 100);
	Walk(LEFT, 10000);
	leftClick(741, 449);
	currentFishIconLoc = make_pair(741, 448);

	fish(acteulLocs);
	leftClick(leaveButton);
}

void vasuMountain()
{
	currentHorrorBaitThreshold = Foamy_Worm;

	goToFishVendor();
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(antiquity);
	leftClick(1478, 221);
	leftClick(yesButton);
	longSleepR(loadTime);
	leftClick(866, 782);
	longSleepR(loadTime);
	Walk(RIGHT, 10000);
	Walk(LEFT, 150);
	Walk(DOWN, 100);
	Walk(LEFT, 10000);
	fightUntilEnd();
	Walk(LEFT, 5000);
	leftClick(727, 442);
	currentFishIconLoc = make_pair(727, 442);

	fish(vasuLocs);
	leftClick(leaveButton);

	currentHorrorBaitThreshold = 9999;
}

void karekSwampland()
{
	goToFishVendor();
	goToBaruoki();
	Walk(RIGHT, 20000);
	WalkUntilBattle();
	Walk(RIGHT, 2000);
	Walk(LEFT, 200);
	Walk(UP, 100);
	Walk(LEFT, 3000);
	Walk(RIGHT, 150, true);
	Walk(UP, 100);
	Walk(RIGHT, 10000);
	fightUntilEnd();
	Walk(RIGHT, 10000);
	WalkUntilBattle();
	Walk(RIGHT, 5000);
	Walk(LEFT, 1400);
	Walk(UP, 100);
	Walk(LEFT, 2000);
	leftClick(623, 448);
	currentFishIconLoc = make_pair(623, 448);

	fish(acteulLocs);
	leftClick(leaveButton);
}

void goToRinde()
{
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(present);
	dragMap(LEFT, 1000);
	dragMap(UP, 600);
	leftClick(598, 303);
	leftClick(yesButton);
	longSleepR(loadTime);
}

void rinde()
{
	goToFishVendor();
	goToRinde();
	Walk(RIGHT, 400);
	Walk(DOWN, 100);
	Walk(RIGHT, 15000);
	leftClick(970, 368);
	currentFishIconLoc = make_pair(970, 368);

	fish(kiraBeachLocs);
	leftClick(leaveButton);
}

void serenaCoast()
{
	currentHorrorBaitThreshold = Snitch_Sardines;

	goToFishVendor();
	goToRinde();
	Walk(LEFT, 20000);
	WalkUntilBattle();
	Walk(LEFT, 5000);
	Walk(RIGHT, 800);
	Walk(UP, 100);
	WalkUntilBattle();
	Walk(RIGHT, 3000);
	Walk(LEFT, 4100);
	Walk(DOWN, 100);
	Walk(LEFT, 3000);
	leftClick(796, 448);
	currentFishIconLoc = make_pair(796, 448);

	fish(acteulLocs);
	leftClick(leaveButton);

	currentHorrorBaitThreshold = 9999;
}

void rucyanaSands()
{
	currentHorrorBaitThreshold = Snitch_Sardines;

	goToFishVendor();
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(1245, 691);
	leftClick(yesButton);
	longSleepR(loadTime);
	leftClick(677, 450);
	longSleepR(5000);
	leftClick(890, 266);
	leftClick(890, 266);
	leftClick(890, 266);
	longSleepR(loadTime + 5000);

	Walk(LEFT, 20000);
	fightUntilEnd();
	Walk(LEFT, 5000);

	leftClick(744, 411);
	currentFishIconLoc = make_pair(744, 411);

	fish(rucyanaSandsLocs);
	leftClick(leaveButton);

	currentHorrorBaitThreshold = 9999;
}

void lastIsland()
{
	useAbsFishLoc = true;
	currentHorrorBaitThreshold = Premium_Crab_Cake;

	goToFishVendor();
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(futur);
	leftClick(1693, 262);
	leftClick(yesButton);
	longSleepR(loadTime);
	Walk(LEFT, 300);
	Walk(UP, 100);
	Walk(RIGHT, 300);

	currentFishIconLoc = findFishIcon();
	leftClick(currentFishIconLoc.first, currentFishIconLoc.second, 2000, false);

	fish(kiraBeachLocs);
	leftClick(leaveButton);

	useAbsFishLoc = false;
	currentHorrorBaitThreshold = 9999;
}

void nilva()
{
	goToFishVendor();
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(futur);
	leftClick(32, 413);
	leftClick(yesButton);
	longSleepR(loadTime);
	Walk(RIGHT, 2200);
	Walk(DOWN, 100);
	Walk(RIGHT, 5000);
	leftClick(1068, 400);
	currentFishIconLoc = make_pair(1068, 400);

	fish(elzionLocs);
	leftClick(leaveButton);
}

void manEatingSwamp()
{
	useAbsFishLoc = true;

	goToFishVendor();
	goToActeul();
	Walk(LEFT, 400);
	Walk(UP, 100);
	Walk(LEFT, 500);
	leftClick(1050, 225);
	leftClick(1050, 225);
	longSleepR(loadTime);
	Walk(RIGHT, 5000);
	Walk(UP, 100);
	Walk(LEFT, 3000);
	leftClick(819, 194);
	longSleepR(loadTime);

	Walk(RIGHT, 2650);

	currentFishIconLoc = findSwampFishIcon();
	leftClick(currentFishIconLoc.first, currentFishIconLoc.second, 2000, false);

	fish(acteulLocs, 5500);
	leftClick(leaveButton);

	useAbsFishLoc = false;
}

void charolPlains()
{
	goToFishVendor();
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(antiquity);
	dragMap(RIGHT, 1700);
	leftClick(897, 103);
	leftClick(yesButton);
	longSleepR(loadTime);
	Walk(LEFT, 1100);
	Walk(DOWN, 100);
	Walk(LEFT, 25000);
	fightUntilEnd();
	Walk(LEFT, 20000);
	leftClick(840, 456);
	currentFishIconLoc = make_pair(840, 456);

	fish(baruokiLocs);
	leftClick(leaveButton);
}

void dimensionRift()
{
	currentHorrorBaitThreshold = Snitch_Sardines;

	goToFishVendor();
	goToSpacetimeRift();
	Walk(LEFT, 1000);
	Walk(UP, 100);
	Walk(LEFT, 10000);
	leftClick(408, 345);
	currentFishIconLoc = make_pair(408, 345);

	fish(dimensionRiftLocs);
	leftClick(leaveButton);

	currentHorrorBaitThreshold = 9999;
}

void goToDragonPalace(bool centralKeep)
{
	goToSpacetimeRift();
	leftClick(mapButton, 3500);
	leftClick(antiquity);
	dragMap(UP, 500);
	leftClick(555, 641);

	if (centralKeep)
		leftClick(1305, 486);
	else
		leftClick(1311, 355);

	leftClick(yesButton);
	longSleepR(loadTime);
}

void outerWallPlum()
{
	goToFishVendor();
	goToDragonPalace(false);
	Walk(LEFT, 5000);
	leftClick(450, 400);
	currentFishIconLoc = make_pair(450, 400);

	fish(dragonPalaceLocs);
	leftClick(leaveButton);
}

void outerWallPlumPast()
{
	goToFishVendor();
	goToDragonPalace(false);

	Walk(LEFT, 500);
	Walk(UP, 100);
	Walk(LEFT, 3300);
	Walk(DOWN, 100);
	Walk(LEFT, 600);
	Walk(UP, 100, true);
	longSleepR(loadTime);
	Walk(RIGHT, 3000);
	leftClick(1040, 120);
	longSleepR(loadTime);
	Walk(LEFT, 100);
	Walk(DOWN, 100, true);
	longSleepR(loadTime);
	Walk(RIGHT, 500);
	Walk(UP, 100);
	Walk(RIGHT, 3300);
	Walk(DOWN, 100);
	Walk(LEFT, 2000);
	leftClick(450, 400);
	currentFishIconLoc = make_pair(450, 400);

	fish(dragonPalaceLocs);
	leftClick(leaveButton);
}

void outerWallBamboo()
{
	goToFishVendor();
	goToDragonPalace(false);

	Walk(LEFT, 500);
	Walk(UP, 100);
	Walk(LEFT, 1100);
	Walk(UP, 100);
	Walk(LEFT, 600);
	Walk(UP, 100, true);
	longSleepR(loadTime);
	Walk(RIGHT, 10000);
	fightUntilEnd();
	Walk(RIGHT, 10000);

	Walk(LEFT, 2000);
	Walk(UP, 100);
	Walk(RIGHT, 5000);
	fightUntilEnd();
	Walk(RIGHT, 5000);
	leftClick(1240, 410);
	currentFishIconLoc = make_pair(1240, 410);

	fish(dragonPalaceLocs);
	leftClick(leaveButton);
}

void outerWallBambooPast()
{
	goToFishVendor();
	goToDragonPalace(false);

	Walk(LEFT, 500);
	Walk(UP, 100);
	Walk(LEFT, 1000);
	Walk(UP, 100);
	Walk(LEFT, 500);
	Walk(UP, 100, true);
	longSleepR(loadTime);


	Walk(RIGHT, 2200);
	Walk(UP, 100);
	Walk(RIGHT, 3000);
	WalkUntilBattle();
	Walk(RIGHT, 2000);
	Walk(LEFT, 200);
	Walk(UP, 100);
	Walk(LEFT, 500);
	leftClick(895, 375);
	longSleepR(loadTime);

	Walk(RIGHT, 100);
	Walk(DOWN, 100);
	Walk(LEFT, 500);
	Walk(DOWN, 100);
	Walk(RIGHT, 2700);
	Walk(UP, 100);
	Walk(RIGHT, 5000);
	WalkUntilBattle();
	Walk(RIGHT, 5000);
	leftClick(1240, 410);
	currentFishIconLoc = make_pair(1240, 410);

	fish(dragonPalaceLocs);
	leftClick(leaveButton);
}

void innerWall()
{
	goToFishVendor();
	goToDragonPalace(true);

	Walk(LEFT, 6000);
	leftClick(751, 407);
	currentFishIconLoc = make_pair(751, 407);

	fish(dragonPalaceLocs);
	leftClick(leaveButton);
}

void innerWallPast()
{
	goToFishVendor();
	goToDragonPalace(true);

	Walk(LEFT, 6000);
	leftClick(534, 406);
	leftClick(yesButton);
	longSleepR(loadTime);
	leftClick(603, 408);
	currentFishIconLoc = make_pair(603, 408);

	fish(dragonPalaceLocs);
	leftClick(leaveButton);
}

void outerWallPine()
{
	goToFishVendor();
	goToDragonPalace(true);

	Walk(LEFT, 1100);
	Walk(DOWN, 100);
	Walk(LEFT, 500);
	Walk(DOWN, 100);
	Walk(RIGHT, 600);
	Walk(DOWN, 100, true);
	longSleepR(loadTime);


	Walk(LEFT, 1200);
	Walk(DOWN, 100);
	Walk(LEFT, 2000);
	WalkUntilBattle();
	Walk(LEFT, 3000);
	leftClick(630, 408);
	currentFishIconLoc = make_pair(630, 408);

	fish(dragonPalaceLocs);
	leftClick(leaveButton);
}

void outerWallPinePast()
{
	goToFishVendor();
	goToDragonPalace(true);

	Walk(LEFT, 6000);
	leftClick(535, 405);
	leftClick(yesButton);
	longSleepR(loadTime);

	Walk(RIGHT, 1500);
	Walk(DOWN, 100);
	Walk(LEFT, 500);
	Walk(DOWN, 100);

	WalkUntilBattle();
	Walk(LEFT, 2000);

	Walk(RIGHT, 1200);
	Walk(DOWN, 100, true);
	longSleepR(loadTime);


	Walk(LEFT, 1200);
	Walk(DOWN, 100);
	Walk(LEFT, 2000);
	leftClick(630, 405);
	currentFishIconLoc = make_pair(630, 405);

	fish(dragonPalaceLocs);
	leftClick(leaveButton);
}

void setup()
{
	string emulator, windowName;

	//Set up baits
	for (int i = Fishing_Dango; i < Premium_Crab_Cake + 1; ++i)
	{
		baitList.push_back(make_pair(false, 0));
	}

	//Read in config file
	string key, value;
	ifstream file("config.txt");
	string str;
	set<int> baitsUsedThisRun;
	auto ltrimString = [&str]()
	{
		str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int ch) {
			return !std::isspace(ch);
		}));
	};
	auto getKeyValue = [&key, &value](string& str)
	{
		int loc = str.find("=");
		key = str.substr(loc + 1);
		value = str.substr(0, loc);
	};
	auto parseBaitForArea = [&ltrimString, &str, &file, &getKeyValue, &key, &value, &baitsUsedThisRun](fishingSpot& area)
	{
		while (1)
		{
			std::getline(file, str);
			ltrimString();
			if (str[0] == '-' || str[0] == '\n') //skip comments and blank lines
				continue;
			if (str.compare("~END") == 0)
				break;

			getKeyValue(str);
			if (stoi(value) > 0)
			{
				if (key.compare("Fishing Dango") == 0)
				{
					area.baitsToUse.push_back(Fishing_Dango);
					baitsUsedThisRun.insert(Fishing_Dango);
				}
				else if (key.compare("Worm") == 0)
				{
					area.baitsToUse.push_back(Worm);
					baitsUsedThisRun.insert(Worm);
				}
				else if (key.compare("Unexpected Worm") == 0)
				{
					area.baitsToUse.push_back(Unexpected_Worm);
					baitsUsedThisRun.insert(Unexpected_Worm);
				}
				else if (key.compare("Shopaholic Clam") == 0)
				{
					area.baitsToUse.push_back(Shopaholic_Clam);
					baitsUsedThisRun.insert(Shopaholic_Clam);
				}
				else if (key.compare("Spree Snail") == 0)
				{
					area.baitsToUse.push_back(Spree_Snail);
					baitsUsedThisRun.insert(Spree_Snail);
				}
				else if (key.compare("Dressed Crab") == 0)
				{
					area.baitsToUse.push_back(Dressed_Crab);
					baitsUsedThisRun.insert(Dressed_Crab);
				}
				else if (key.compare("Tear Crab") == 0)
				{
					area.baitsToUse.push_back(Tear_Crab);
					baitsUsedThisRun.insert(Tear_Crab);
				}
				else if (key.compare("Foamy Worm") == 0)
				{
					area.baitsToUse.push_back(Foamy_Worm);
					baitsUsedThisRun.insert(Foamy_Worm);
				}
				else if (key.compare("Bubbly Worm") == 0)
				{
					area.baitsToUse.push_back(Bubbly_Worm);
					baitsUsedThisRun.insert(Bubbly_Worm);
				}
				else if (key.compare("Snitch Sardines") == 0)
				{
					area.baitsToUse.push_back(Snitch_Sardines);
					baitsUsedThisRun.insert(Snitch_Sardines);
				}
				else if (key.compare("Babbling Sardines") == 0)
				{
					area.baitsToUse.push_back(Babbling_Sardines);
					baitsUsedThisRun.insert(Babbling_Sardines);
				}
				else if (key.compare("Crab Cake") == 0)
				{
					area.baitsToUse.push_back(Crab_Cake);
					baitsUsedThisRun.insert(Crab_Cake);
				}
				else if (key.compare("Premium Crab Cake") == 0)
				{
					area.baitsToUse.push_back(Premium_Crab_Cake);
					baitsUsedThisRun.insert(Premium_Crab_Cake);
				}
			}
		}
	};

	vector<pair<int, void(*)(void)>> fishingSpotsInput;
	bool inArea = false;
	while (std::getline(file, str))
	{
		if (str[0] == '-' || str[0] == '\n') //skip comments and blank lines
			continue;

		getKeyValue(str);

		if (key.compare("Fishing Dango Max") == 0)
			baitList[Fishing_Dango].second = stoi(value);
		else if (key.compare("Worm Max") == 0)
			baitList[Worm].second = stoi(value);
		else if (key.compare("Unexpected Worm Max") == 0)
			baitList[Unexpected_Worm].second = stoi(value);
		else if (key.compare("Shopaholic Clam Max") == 0)
			baitList[Shopaholic_Clam].second = stoi(value);
		else if (key.compare("Spree Snail Max") == 0)
			baitList[Spree_Snail].second = stoi(value);
		else if (key.compare("Dressed Crab Max") == 0)
			baitList[Dressed_Crab].second = stoi(value);
		else if (key.compare("Tear Crab Max") == 0)
			baitList[Tear_Crab].second = stoi(value);
		else if (key.compare("Foamy Worm Max") == 0)
			baitList[Foamy_Worm].second = stoi(value);
		else if (key.compare("Bubbly Worm Max") == 0)
			baitList[Bubbly_Worm].second = stoi(value);
		else if (key.compare("Snitch Sardines Max") == 0)
			baitList[Snitch_Sardines].second = stoi(value);
		else if (key.compare("Babbling Sardines Max") == 0)
			baitList[Babbling_Sardines].second = stoi(value);
		else if (key.compare("Crab Cake Max") == 0)
			baitList[Crab_Cake].second = stoi(value);
		else if (key.compare("Premium Crab Cake Max") == 0)
			baitList[Premium_Crab_Cake].second = stoi(value);


		else if (key.compare("Baruoki") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&baruoki, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Acteul") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&acteul, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Charol Plains") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&charolPlains, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Elzion Airport") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&elzionAirport, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Dimension Rift") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&dimensionRift, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Dragon Palace - Inner Wall") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&innerWall, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Dragon Palace Past - Inner Wall") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&innerWallPast, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Dragon Palace - Outer Wall Bamboo") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&outerWallBamboo, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Dragon Palace Past - Outer Wall Bamboo") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&outerWallBambooPast, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Dragon Palace - Outer Wall Pine") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&outerWallPine, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Dragon Palace Past - Outer Wall Pine") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&outerWallPinePast, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Dragon Palace - Outer Wall Plum") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&outerWallPlum, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Dragon Palace Past - Outer Wall Plum") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&outerWallPlumPast, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Karek Swampland") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&karekSwampland, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Kira Beach") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&kiraBeach, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Lake Tillian") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&lakeTillian, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Last Island") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&lastIsland, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Man-Eating Swamp") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&manEatingSwamp, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Naaru Uplands") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&naaruUplands, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Nilva") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&nilva, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Rinde") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&rinde, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Rucyana Sands") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&rucyanaSands, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Serena Coast") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&serenaCoast, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		else if (key.compare("Vasu Mountains") == 0 && stoi(value) > 0)
		{
			fishingSpots.push_back(fishingSpot(&vasuMountain, stoi(value)));
			parseBaitForArea(fishingSpots.back());
		}
		//else if (key.compare("Emulator") == 0)
		//{
		//	emulator = value;
		//}
		else if (key.compare("Window Name") == 0)
		{
			windowName = value;
		}
		else if (key.compare("Load Time") == 0)
		{
			loadTime = stoi(value);
		}
	}

	ocr = OCRTesseract::create(NULL, NULL, NULL, OEM_TESSERACT_ONLY, PSM_SINGLE_LINE);

	rng = std::mt19937(dev());
	lClickRand = std::uniform_int_distribution<std::mt19937::result_type>(0, 10);
	sleepRand = std::uniform_int_distribution<std::mt19937::result_type>(0, 50);
	slideSleepRand = std::uniform_int_distribution<std::mt19937::result_type>(0, 1);
	slideLClick = std::uniform_int_distribution<std::mt19937::result_type>(0, 100);
	slideRand = std::uniform_int_distribution<std::mt19937::result_type>(0, 2);
	boolRand = std::uniform_int_distribution<std::mt19937::result_type>(0, 1);
	slideDistanceRand = std::uniform_int_distribution<std::mt19937::result_type>(0, 100);
	longSleepRand = std::uniform_int_distribution<std::mt19937::result_type>(0, 1000);

	string innerWindowName;
	//if (emulator.compare("LD") == 0)
	//{
	emulator = "dnplayer.exe";
	innerWindowName = "TheRender";
	//}
	//else
	//{
		//emulator = "Nox.exe";
		//innerWindowName = "ScreenBoardClassWindow";
	//}

	pair<string*, string*> enumInput = make_pair(&emulator, &windowName);
	EnumWindows(EnumWindowsProc, LPARAM(&enumInput));
	EnumChildWindows(window, EnumChildWindowsProc, LPARAM(&innerWindowName));

	RECT rect;
	GetWindowRect(window, &rect);
	height = rect.bottom - rect.top;
	width = rect.right - rect.left;
	xCenter = width / 2;
	yCenter = height / 2;
	heightPct = height / 981.0;
	widthPct = width / 1745.0;

	Mat origFishIcon = imread("fish.png", IMREAD_UNCHANGED);
	Mat origExclamationIcon = imread("exclamation.png", IMREAD_UNCHANGED);
	Mat origSwampFishIcon = imread("swampFish.png", IMREAD_UNCHANGED);
	if (heightPct != 100.0 || widthPct != 100.0)
	{
		resize(origFishIcon, fishIcon, Size(), widthPct, heightPct, heightPct >= 1 ? INTER_CUBIC : INTER_AREA);
		resize(origExclamationIcon, exclamationIcon, Size(), widthPct, heightPct, heightPct >= 1 ? INTER_CUBIC : INTER_AREA);
		resize(origSwampFishIcon, swampFishIcon, Size(), widthPct, heightPct, heightPct >= 1 ? INTER_CUBIC : INTER_AREA);
	}

	hdc = GetWindowDC(window);
	hDest = CreateCompatibleDC(hdc);

	void *ptrBitmapPixels;

	BITMAPINFO bi;
	ZeroMemory(&bi, sizeof(BITMAPINFO));
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = width;
	bi.bmiHeader.biHeight = -height;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	hbDesktop = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &ptrBitmapPixels, NULL, 0);

	SelectObject(hDest, hbDesktop);

	bitbltPic = Mat(height, width, CV_8UC4, ptrBitmapPixels, 0);

	std::sort(fishingSpots.begin(), fishingSpots.end(), [](fishingSpot& lhs, fishingSpot& rhs) { return lhs.orderNumber < rhs.orderNumber; });

	//Check to see if any spots actually use the bait we've selected to buy. If not, don't buy any for this run
	for (int i = 0; i < baitList.size(); ++i)
	{
		if (baitsUsedThisRun.find(i) == baitsUsedThisRun.end())
			baitList[i].second = 0;
	}
}

int main()
{
	setup();

	while (1)
	{
		for (int i = 0; i < fishingSpots.size(); ++i)
		{
			currentBaitsToUse = &(fishingSpots[i].baitsToUse);
			(*fishingSpots[i].fishFunction)();
		}
	}
}