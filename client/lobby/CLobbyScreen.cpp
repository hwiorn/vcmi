/*
 * CLobbyScreen.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "CLobbyScreen.h"
#include "CBonusSelection.h"
#include "SelectionTab.h"
#include "RandomMapTab.h"
#include "OptionsTab.h"
#include "../CServerHandler.h"

#include "../gui/CGuiHandler.h"
#include "../widgets/Buttons.h"
#include "../widgets/Images.h"
#include "../widgets/TextControls.h"
#include "../windows/InfoWindows.h"

#include "../../CCallback.h"

#include "../CGameInfo.h"
#include "../../lib/NetPacksLobby.h"
#include "../../lib/CGeneralTextHandler.h"
#include "../../lib/mapping/CMapInfo.h"
#include "../../lib/mapping/CCampaignHandler.h"
#include "../../lib/rmg/CMapGenOptions.h"

CLobbyScreen::CLobbyScreen(ESelectionScreen screenType)
	: CSelectionBase(screenType), bonusSel(nullptr)
{
	OBJ_CONSTRUCTION_CAPTURING_ALL_NO_DISPOSE;
	tabSel = std::make_shared<SelectionTab>(screenType);
	curTab = tabSel;

	auto initLobby = [&]()
	{
		tabSel->callOnSelect = std::bind(&IServerAPI::setMapInfo, CSH, _1, nullptr);

		buttonSelect = std::make_shared<CButton>(Point(411, 80), "GSPBUTT.DEF", CGI->generaltexth->zelp[45], 0, SDLK_s);
		buttonSelect->addCallback([&]()
		{
			toggleTab(tabSel);
			CSH->setMapInfo(tabSel->getSelectedMapInfo());
		});

		buttonOptions = std::make_shared<CButton>(Point(411, 510), "GSPBUTT.DEF", CGI->generaltexth->zelp[46], std::bind(&CLobbyScreen::toggleTab, this, tabOpt), SDLK_a);
	};

	buttonChat = std::make_shared<CButton>(Point(619, 83), "GSPBUT2.DEF", CGI->generaltexth->zelp[48], std::bind(&CLobbyScreen::toggleChat, this), SDLK_h);
	buttonChat->addTextOverlay(CGI->generaltexth->allTexts[532], FONT_SMALL);

	switch(screenType)
	{
	case ESelectionScreen::newGame:
	{
		tabOpt = std::make_shared<OptionsTab>();
		tabRand = std::make_shared<RandomMapTab>();
		tabRand->mapInfoChanged += std::bind(&IServerAPI::setMapInfo, CSH, _1, _2);
		buttonRMG = std::make_shared<CButton>(Point(411, 105), "GSPBUTT.DEF", CGI->generaltexth->zelp[47], 0, SDLK_r);
		buttonRMG->addCallback([&]()
		{
			toggleTab(tabRand);
			tabRand->updateMapInfoByHost(); // TODO: This is only needed to force-update mapInfo in CSH when tab is opened
		});

		card->iconDifficulty->addCallback(std::bind(&IServerAPI::setDifficulty, CSH, _1));

		buttonStart = std::make_shared<CButton>(Point(411, 535), "SCNRBEG.DEF", CGI->generaltexth->zelp[103], std::bind(&CLobbyScreen::startScenario, this, false), SDLK_b);
		initLobby();
		break;
	}
	case ESelectionScreen::loadGame:
	{
		buttonMods = std::make_shared<CButton>(Point(619, 105), "GSPBUT2.DEF", std::make_pair<std::string, std::string>("", ""), [](){ GH.pushInt(new CModsBox()); }, SDLK_h);
		buttonMods->addTextOverlay(CGI->generaltexth->localizedTexts["lobby"]["buttonMods"]["default"].String(), EFonts::FONT_SMALL, Colors::WHITE);
		tabOpt = std::make_shared<OptionsTab>();
		buttonStart = std::make_shared<CButton>(Point(411, 535), "SCNRLOD.DEF", CGI->generaltexth->zelp[103], std::bind(&CLobbyScreen::startScenario, this, false), SDLK_l);
		initLobby();
		break;
	}
	case ESelectionScreen::campaignList:
		tabSel->callOnSelect = std::bind(&IServerAPI::setMapInfo, CSH, _1, nullptr);
		buttonStart = std::make_shared<CButton>(Point(411, 535), "SCNRLOD.DEF", CButton::tooltip(), std::bind(&CLobbyScreen::startCampaign, this), SDLK_b);
		break;
	}

	buttonStart->assignedKeys.insert(SDLK_RETURN);

	buttonBack = std::make_shared<CButton>(Point(581, 535), "SCNRBACK.DEF", CGI->generaltexth->zelp[105], [&](){CSH->sendClientDisconnecting(); GH.popIntTotally(this);}, SDLK_ESCAPE);
}

CLobbyScreen::~CLobbyScreen()
{
	// TODO: For now we always destroy whole lobby when leaving bonus selection screen
	if(CSH->state == EClientState::LOBBY_CAMPAIGN)
		CSH->sendClientDisconnecting();
}

void CLobbyScreen::toggleTab(std::shared_ptr<CIntObject> tab)
{
	if(tab == curTab)
		CSH->sendGuiAction(LobbyGuiAction::NO_TAB);
	else if(tab == tabOpt)
		CSH->sendGuiAction(LobbyGuiAction::OPEN_OPTIONS);
	else if(tab == tabSel)
		CSH->sendGuiAction(LobbyGuiAction::OPEN_SCENARIO_LIST);
	else if(tab == tabRand)
		CSH->sendGuiAction(LobbyGuiAction::OPEN_RANDOM_MAP_OPTIONS);
	CSelectionBase::toggleTab(tab);
}

void CLobbyScreen::startCampaign()
{
	if(CSH->mi)
	{
		auto ourCampaign = std::make_shared<CCampaignState>(CCampaignHandler::getCampaign(CSH->mi->fileURI));
		CSH->setCampaignState(ourCampaign);
	}
}

void CLobbyScreen::startScenario(bool allowOnlyAI)
{
	try
	{
		CSH->sendStartGame(allowOnlyAI);
		buttonStart->block(true);
	}
	catch(ExceptionMapMissing & e)
	{

	}
	catch(ExceptionNoHuman & e)
	{
		// You must position yourself prior to starting the game.
		CInfoWindow::showYesNoDialog(std::ref(CGI->generaltexth->allTexts[530]), CInfoWindow::TCompsInfo(), 0, std::bind(&CLobbyScreen::startScenario, this, true), PlayerColor(1));
	}
	catch(ExceptionNoTemplate & e)
	{
		GH.pushInt(CInfoWindow::create(CGI->generaltexth->allTexts[751]));
	}
	catch(...)
	{

	}
}

void CLobbyScreen::toggleMode(bool host)
{
	tabSel->toggleMode();
	buttonStart->block(!host);
	if(screenType == ESelectionScreen::campaignList)
		return;

	auto buttonColor = host ? Colors::WHITE : Colors::ORANGE;
	buttonSelect->addTextOverlay(CGI->generaltexth->allTexts[500], FONT_SMALL, buttonColor);
	buttonOptions->addTextOverlay(CGI->generaltexth->allTexts[501], FONT_SMALL, buttonColor);
	if(buttonRMG)
	{
		buttonRMG->addTextOverlay(CGI->generaltexth->allTexts[740], FONT_SMALL, buttonColor);
		buttonRMG->block(!host);
	}
	buttonSelect->block(!host);
	buttonOptions->block(!host);

	if(CSH->mi)
		tabOpt->recreate();
}

void CLobbyScreen::toggleChat()
{
	card->toggleChat();
	if(card->showChat)
		buttonChat->addTextOverlay(CGI->generaltexth->allTexts[531], FONT_SMALL);
	else
		buttonChat->addTextOverlay(CGI->generaltexth->allTexts[532], FONT_SMALL);
}

void CLobbyScreen::updateAfterStateChange()
{
	if(CSH->mi)
	{
		if(tabOpt)
			tabOpt->recreate();

		if(CSH->mi->scenarioOptionsOfSave && CSH->mi->scenarioOptionsOfSave->mods.size())
		{
			buttonMods->enable();
			auto erroredMods = CGI->modh->checkModsPresent(CSH->mi->scenarioOptionsOfSave->mods);
			if(erroredMods.size())
			{
				buttonMods->addTextOverlay(CGI->generaltexth->localizedTexts["lobby"]["buttonMods"]["error"].String(), EFonts::FONT_SMALL, Colors::ORANGE);
			}
			else
			{
				buttonMods->addTextOverlay(CGI->generaltexth->localizedTexts["lobby"]["buttonMods"]["default"].String(), EFonts::FONT_SMALL, Colors::WHITE);
			}
		}
		else if(buttonMods)
		{
			buttonMods->disable();
		}
	}

	card->changeSelection();
	if(card->iconDifficulty)
		card->iconDifficulty->setSelected(CSH->si->difficulty);

	if(curTab == tabRand && CSH->si->mapGenOptions)
		tabRand->setMapGenOptions(CSH->si->mapGenOptions);
}

const StartInfo * CLobbyScreen::getStartInfo()
{
	return CSH->si.get();
}

const CMapInfo * CLobbyScreen::getMapInfo()
{
	return CSH->mi.get();
}

CModsBox::CModsBox()
	: CWindowObject(BORDERED | SHADOW_DISABLED, "DIBOXBCK")
{
	OBJ_CONSTRUCTION_CAPTURING_ALL_NO_DISPOSE;
	pos.w = 256;
	int lineHeight = graphics->fonts[FONT_SMALL]->getLineHeight();
	pos.h = 90 + 50 * 3 + lineHeight * CSH->mi->scenarioOptionsOfSave->mods.size();

	labelTitle = std::make_shared<CLabel>(128, 30, FONT_MEDIUM, CENTER, Colors::YELLOW, CGI->generaltexth->localizedTexts["lobby"]["windowMods"]["title"].String());
	labelGroupStatus = std::make_shared<CLabelGroup>(FONT_SMALL, EAlignment::CENTER, Colors::WHITE);
	labelGroupMods = std::make_shared<CLabelGroup>(FONT_SMALL, EAlignment::CENTER, Colors::WHITE);
	auto erroredMods = CGI->modh->checkModsPresent(CSH->mi->scenarioOptionsOfSave->mods);

	for(int i = 0; i < 3; i++)
	{
		std::vector<ui8> flags;
		if(i == 0)
			labelGroupStatus->add(128, 65 + 50 * i, CGI->generaltexth->localizedTexts["lobby"]["windowMods"]["groups"]["working"].String());
		if(i == 1)
			labelGroupStatus->add(128, 65 + 50 * i + lineHeight * labelGroupMods->currentSize(), CGI->generaltexth->localizedTexts["lobby"]["windowMods"]["groups"]["missing"].String());
		if(i == 2)
			labelGroupStatus->add(128, 65 + 50 * i + lineHeight * labelGroupMods->currentSize(), CGI->generaltexth->localizedTexts["lobby"]["windowMods"]["groups"]["incompatible"].String());

		for(auto & mod : CSH->mi->scenarioOptionsOfSave->mods)
		{
			if(!vstd::contains(erroredMods, mod.identifier))
			{
				if(i == 0)
					labelGroupMods->add(128, 75 + 50 * i + lineHeight * labelGroupMods->currentSize(), mod.name);

			}
			else
			{
				if(i == 1 && erroredMods[mod.identifier] == CModInfo::MISSING)
				{
					labelGroupMods->add(128, 75 + 50 * i + lineHeight * labelGroupMods->currentSize(), mod.name);
				}
				else if(i == 2 && erroredMods[mod.identifier] == CModInfo::INCOMPATIBLE)
				{
					labelGroupMods->add(128, 75 + 50 * i + lineHeight * labelGroupMods->currentSize(), mod.name);
				}
			}
		}

		for(int j = 0; j < PlayerColor::PLAYER_LIMIT_I; j++)
		{
			if((SEL->getPlayerInfo(j).canHumanPlay || SEL->getPlayerInfo(j).canComputerPlay)
				&& SEL->getPlayerInfo(j).team == TeamID(i))
			{
				flags.push_back(j);
			}
		}
	}
	buttonCancel = std::make_shared<CButton>(Point(100, pos.h-50), "MUBCANC.DEF", CGI->generaltexth->zelp[561], std::bind(&CGuiHandler::popIntTotally, &GH, this), SDLK_ESCAPE);

	background->scaleTo(Point(pos.w, pos.h));
	center();
}