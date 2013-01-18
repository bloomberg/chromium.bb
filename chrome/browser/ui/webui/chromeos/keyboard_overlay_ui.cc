// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/keyboard_overlay_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using chromeos::input_method::ModifierKey;
using content::WebUIMessageHandler;
using ui::WebDialogUI;

namespace {

const char kLearnMoreURL[] =
#if defined(OFFICIAL_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/"
    "main.html?answer=188743";
#else
    "http://support.google.com/chromeos/bin/answer.py?answer=183101";
#endif

struct ModifierToLabel {
  const ModifierKey modifier;
  const char* label;
} kModifierToLabels[] = {
  {chromeos::input_method::kSearchKey, "search"},
  {chromeos::input_method::kControlKey, "ctrl"},
  {chromeos::input_method::kAltKey, "alt"},
  {chromeos::input_method::kVoidKey, "disabled"},
  {chromeos::input_method::kCapsLockKey, "caps lock"},
};

struct I18nContentToMessage {
  const char* i18n_content;
  int message;
} kI18nContentToMessage[] = {
  { "keyboardOverlayLearnMore", IDS_KEYBOARD_OVERLAY_LEARN_MORE },
  { "keyboardOverlayTitle", IDS_KEYBOARD_OVERLAY_TITLE },
  { "keyboardOverlayF1", IDS_KEYBOARD_OVERLAY_F1 },
  { "keyboardOverlayF2", IDS_KEYBOARD_OVERLAY_F2 },
  { "keyboardOverlayF3", IDS_KEYBOARD_OVERLAY_F3 },
  { "keyboardOverlayF4", IDS_KEYBOARD_OVERLAY_F4 },
  { "keyboardOverlayF5", IDS_KEYBOARD_OVERLAY_F5 },
  { "keyboardOverlayF6", IDS_KEYBOARD_OVERLAY_F6 },
  { "keyboardOverlayF7", IDS_KEYBOARD_OVERLAY_F7 },
  { "keyboardOverlayF8", IDS_KEYBOARD_OVERLAY_F8 },
  { "keyboardOverlayF9", IDS_KEYBOARD_OVERLAY_F9 },
  { "keyboardOverlayF10", IDS_KEYBOARD_OVERLAY_F10 },
  { "keyboardOverlayF11", IDS_KEYBOARD_OVERLAY_F11 },
  { "keyboardOverlayF12", IDS_KEYBOARD_OVERLAY_F12 },
  { "keyboardOverlayInsert", IDS_KEYBOARD_OVERLAY_INSERT },
  { "keyboardOverlayInstructions", IDS_KEYBOARD_OVERLAY_INSTRUCTIONS },
  { "keyboardOverlayInstructionsHide", IDS_KEYBOARD_OVERLAY_INSTRUCTIONS_HIDE },
  { "keyboardOverlayActivateLastLauncherItem",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_LAST_LAUNCHER_ITEM },
  { "keyboardOverlayActivateLastTab", IDS_KEYBOARD_OVERLAY_ACTIVATE_LAST_TAB },
  { "keyboardOverlayActivateLauncherItem1",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_LAUNCHER_ITEM_1 },
  { "keyboardOverlayActivateLauncherItem2",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_LAUNCHER_ITEM_2 },
  { "keyboardOverlayActivateLauncherItem3",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_LAUNCHER_ITEM_3 },
  { "keyboardOverlayActivateLauncherItem4",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_LAUNCHER_ITEM_4 },
  { "keyboardOverlayActivateLauncherItem5",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_LAUNCHER_ITEM_5 },
  { "keyboardOverlayActivateLauncherItem6",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_LAUNCHER_ITEM_6 },
  { "keyboardOverlayActivateLauncherItem7",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_LAUNCHER_ITEM_7 },
  { "keyboardOverlayActivateLauncherItem8",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_LAUNCHER_ITEM_8 },
  { "keyboardOverlayActivateNextTab", IDS_KEYBOARD_OVERLAY_ACTIVATE_NEXT_TAB },
  { "keyboardOverlayActivatePreviousTab",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_PREVIOUS_TAB },
  { "keyboardOverlayActivateTab1", IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_1 },
  { "keyboardOverlayActivateTab2", IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_2 },
  { "keyboardOverlayActivateTab3", IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_3 },
  { "keyboardOverlayActivateTab4", IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_4 },
  { "keyboardOverlayActivateTab5", IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_5 },
  { "keyboardOverlayActivateTab6", IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_6 },
  { "keyboardOverlayActivateTab7", IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_7 },
  { "keyboardOverlayActivateTab8", IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_8 },
  { "keyboardOverlayAddWwwAndComAndOpenAddress",
    IDS_KEYBOARD_OVERLAY_ADD_WWW_AND_COM_AND_OPEN_ADDRESS },
  { "keyboardOverlayBookmarkAllTabs", IDS_KEYBOARD_OVERLAY_BOOKMARK_ALL_TABS },
  { "keyboardOverlayBookmarkCurrentPage",
    IDS_KEYBOARD_OVERLAY_BOOKMARK_CURRENT_PAGE },
  { "keyboardOverlayBookmarkManager", IDS_KEYBOARD_OVERLAY_BOOKMARK_MANAGER },
  { "keyboardOverlayCenterWindow", IDS_KEYBOARD_OVERLAY_CENTER_WINDOW },
  { "keyboardOverlayClearBrowsingDataDialog",
    IDS_KEYBOARD_OVERLAY_CLEAR_BROWSING_DATA_DIALOG },
  { "keyboardOverlayCloseTab", IDS_KEYBOARD_OVERLAY_CLOSE_TAB },
  { "keyboardOverlayCloseWindow", IDS_KEYBOARD_OVERLAY_CLOSE_WINDOW },
  { "keyboardOverlayCopy", IDS_KEYBOARD_OVERLAY_COPY },
  { "keyboardOverlayCut", IDS_KEYBOARD_OVERLAY_CUT },
  { "keyboardOverlayCycleThroughInputMethods",
    IDS_KEYBOARD_OVERLAY_CYCLE_THROUGH_INPUT_METHODS },
  { "keyboardOverlayDecreaseKeyBrightness",
    IDS_KEYBOARD_OVERLAY_DECREASE_KEY_BRIGHTNESS },
  { "keyboardOverlayDelete", IDS_KEYBOARD_OVERLAY_DELETE },
  { "keyboardOverlayDeleteWord", IDS_KEYBOARD_OVERLAY_DELETE_WORD },
  { "keyboardOverlayDeveloperTools", IDS_KEYBOARD_OVERLAY_DEVELOPER_TOOLS },
  { "keyboardOverlayDockWindowLeft", IDS_KEYBOARD_OVERLAY_DOCK_WINDOW_LEFT },
  { "keyboardOverlayDockWindowRight", IDS_KEYBOARD_OVERLAY_DOCK_WINDOW_RIGHT },
  { "keyboardOverlayDomInspector", IDS_KEYBOARD_OVERLAY_DOM_INSPECTOR },
  { "keyboardOverlayDownloads", IDS_KEYBOARD_OVERLAY_DOWNLOADS },
  { "keyboardOverlayEnd", IDS_KEYBOARD_OVERLAY_END },
  { "keyboardOverlayFindPreviousText",
    IDS_KEYBOARD_OVERLAY_FIND_PREVIOUS_TEXT },
  { "keyboardOverlayFindText", IDS_KEYBOARD_OVERLAY_FIND_TEXT },
  { "keyboardOverlayFindTextAgain", IDS_KEYBOARD_OVERLAY_FIND_TEXT_AGAIN },
  { "keyboardOverlayFocusAddressBar", IDS_KEYBOARD_OVERLAY_FOCUS_ADDRESS_BAR },
  { "keyboardOverlayFocusAddressBarInSearchMode",
    IDS_KEYBOARD_OVERLAY_FOCUS_ADDRESS_BAR_IN_SEARCH_MODE },
  { "keyboardOverlayFocusBookmarks", IDS_KEYBOARD_OVERLAY_FOCUS_BOOKMARKS },
  { "keyboardOverlayFocusLauncher", IDS_KEYBOARD_OVERLAY_FOCUS_LAUNCHER },
  { "keyboardOverlayFocusNextPane", IDS_KEYBOARD_OVERLAY_FOCUS_NEXT_PANE },
  { "keyboardOverlayFocusPreviousPane",
    IDS_KEYBOARD_OVERLAY_FOCUS_PREVIOUS_PANE },
  { "keyboardOverlayFocusToolbar", IDS_KEYBOARD_OVERLAY_FOCUS_TOOLBAR },
  { "keyboardOverlayFullScreen", IDS_KEYBOARD_OVERLAY_FULL_SCREEN },
  { "keyboardOverlayGoBack", IDS_KEYBOARD_OVERLAY_GO_BACK },
  { "keyboardOverlayGoForward", IDS_KEYBOARD_OVERLAY_GO_FORWARD },
  { "keyboardOverlayHelp", IDS_KEYBOARD_OVERLAY_HELP },
  { "keyboardOverlayHistory", IDS_KEYBOARD_OVERLAY_HISTORY },
  { "keyboardOverlayHome", IDS_KEYBOARD_OVERLAY_HOME },
  { "keyboardOverlayIncreaseKeyBrightness",
    IDS_KEYBOARD_OVERLAY_INCREASE_KEY_BRIGHTNESS },
  { "keyboardOverlayInputUnicodeCharacters",
    IDS_KEYBOARD_OVERLAY_INPUT_UNICODE_CHARACTERS },
  { "keyboardOverlayJavascriptConsole",
    IDS_KEYBOARD_OVERLAY_JAVASCRIPT_CONSOLE },
  { "keyboardOverlayLockScreen", IDS_KEYBOARD_OVERLAY_LOCK_SCREEN },
  { "keyboardOverlayLockScreenOrPowerOff",
    IDS_KEYBOARD_OVERLAY_LOCK_SCREEN_OR_POWER_OFF },
  { "keyboardOverlayMagnifierDecreaseZoom",
    IDS_KEYBOARD_OVERLAY_MAGNIFIER_DECREASE_ZOOM },
  { "keyboardOverlayMagnifierIncreaseZoom",
    IDS_KEYBOARD_OVERLAY_MAGNIFIER_INCREASE_ZOOM },
  { "keyboardOverlayMaximizeWindow", IDS_KEYBOARD_OVERLAY_MAXIMIZE_WINDOW },
  { "keyboardOverlayMinimizeWindow", IDS_KEYBOARD_OVERLAY_MINIMIZE_WINDOW },
  { "keyboardOverlayMirrorMonitors", IDS_KEYBOARD_OVERLAY_MIRROR_MONITORS },
  { "keyboardOverlayNewIncognitoWindow",
    IDS_KEYBOARD_OVERLAY_NEW_INCOGNITO_WINDOW },
  { "keyboardOverlayNewTab", IDS_KEYBOARD_OVERLAY_NEW_TAB },
  { "keyboardOverlayNewTerminal", IDS_KEYBOARD_OVERLAY_NEW_TERMINAL },
  { "keyboardOverlayNewWindow", IDS_KEYBOARD_OVERLAY_NEW_WINDOW },
  { "keyboardOverlayNextWindow", IDS_KEYBOARD_OVERLAY_NEXT_WINDOW },
  { "keyboardOverlayNextWord", IDS_KEYBOARD_OVERLAY_NEXT_WORD },
  { "keyboardOverlayOpenAddressInNewTab",
    IDS_KEYBOARD_OVERLAY_OPEN_ADDRESS_IN_NEW_TAB },
  { "keyboardOverlayOpenDialog", IDS_KEYBOARD_OVERLAY_OPEN_DIALOG },
  { "keyboardOverlayPageDown", IDS_KEYBOARD_OVERLAY_PAGE_DOWN },
  { "keyboardOverlayPageUp", IDS_KEYBOARD_OVERLAY_PAGE_UP },
  { "keyboardOverlayPaste", IDS_KEYBOARD_OVERLAY_PASTE },
  { "keyboardOverlayPasteAsPlainText",
    IDS_KEYBOARD_OVERLAY_PASTE_AS_PLAIN_TEXT },
  { "keyboardOverlayPreviousWindow", IDS_KEYBOARD_OVERLAY_PREVIOUS_WINDOW },
  { "keyboardOverlayPreviousWord", IDS_KEYBOARD_OVERLAY_PREVIOUS_WORD },
  { "keyboardOverlayPrint", IDS_KEYBOARD_OVERLAY_PRINT },
  { "keyboardOverlayReloadCurrentPage",
    IDS_KEYBOARD_OVERLAY_RELOAD_CURRENT_PAGE },
  { "keyboardOverlayReloadIgnoringCache",
    IDS_KEYBOARD_OVERLAY_RELOAD_IGNORING_CACHE },
  { "keyboardOverlayReopenLastClosedTab",
    IDS_KEYBOARD_OVERLAY_REOPEN_LAST_CLOSED_TAB },
  { "keyboardOverlayReportIssue", IDS_KEYBOARD_OVERLAY_REPORT_ISSUE },
  { "keyboardOverlayResetZoom", IDS_KEYBOARD_OVERLAY_RESET_ZOOM },
  { "keyboardOverlaySave", IDS_KEYBOARD_OVERLAY_SAVE },
  { "keyboardOverlayScreenshotRegion",
    IDS_KEYBOARD_OVERLAY_SCREENSHOT_REGION },
  { "keyboardOverlayScrollUpOnePage",
    IDS_KEYBOARD_OVERLAY_SCROLL_UP_ONE_PAGE },
  { "keyboardOverlaySelectAll", IDS_KEYBOARD_OVERLAY_SELECT_ALL },
  { "keyboardOverlaySelectPreviousInputMethod",
    IDS_KEYBOARD_OVERLAY_SELECT_PREVIOUS_INPUT_METHOD },
  { "keyboardOverlaySelectWordAtATime",
    IDS_KEYBOARD_OVERLAY_SELECT_WORD_AT_A_TIME },
  { "keyboardOverlayShowStatusMenu", IDS_KEYBOARD_OVERLAY_SHOW_STATUS_MENU },
  { "keyboardOverlayShowWrenchMenu", IDS_KEYBOARD_OVERLAY_SHOW_WRENCH_MENU },
  { "keyboardOverlaySignOut", IDS_KEYBOARD_OVERLAY_SIGN_OUT },
  { "keyboardOverlaySwapPrimaryMonitor",
    IDS_KEYBOARD_OVERLAY_SWAP_PRIMARY_MONITOR },
  { "keyboardOverlayTakeScreenshot", IDS_KEYBOARD_OVERLAY_TAKE_SCREENSHOT },
  { "keyboardOverlayTaskManager", IDS_KEYBOARD_OVERLAY_TASK_MANAGER },
  { "keyboardOverlayToggleBookmarkBar",
    IDS_KEYBOARD_OVERLAY_TOGGLE_BOOKMARK_BAR },
  { "keyboardOverlayToggleCapsLock", IDS_KEYBOARD_OVERLAY_TOGGLE_CAPS_LOCK },
  { "keyboardOverlayToggleSpeechInput",
    IDS_KEYBOARD_OVERLAY_TOGGLE_SPEECH_INPUT },
  { "keyboardOverlayToggleSpokenFeedback",
    IDS_KEYBOARD_OVERLAY_TOGGLE_SPOKEN_FEEDBACK },
  { "keyboardOverlayUndo", IDS_KEYBOARD_OVERLAY_UNDO },
  { "keyboardOverlayViewKeyboardOverlay",
    IDS_KEYBOARD_OVERLAY_VIEW_KEYBOARD_OVERLAY },
  { "keyboardOverlayViewSource", IDS_KEYBOARD_OVERLAY_VIEW_SOURCE },
  { "keyboardOverlayWordMove", IDS_KEYBOARD_OVERLAY_WORD_MOVE },
  { "keyboardOverlayZoomIn", IDS_KEYBOARD_OVERLAY_ZOOM_IN },
  { "keyboardOverlayZoomOut", IDS_KEYBOARD_OVERLAY_ZOOM_OUT },
};

std::string ModifierKeyToLabel(ModifierKey modifier) {
  for (size_t i = 0; i < arraysize(kModifierToLabels); ++i) {
    if (modifier == kModifierToLabels[i].modifier) {
      return kModifierToLabels[i].label;
    }
  }
  return "";
}

ChromeWebUIDataSource* CreateKeyboardOverlayUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIKeyboardOverlayHost);

  for (size_t i = 0; i < arraysize(kI18nContentToMessage); ++i) {
    source->AddLocalizedString(kI18nContentToMessage[i].i18n_content,
                               kI18nContentToMessage[i].message);
  }

  source->AddString("keyboardOverlayLearnMoreURL", UTF8ToUTF16(kLearnMoreURL));
  const char* has_diamond_key_value =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kHasChromeOSDiamondKey) ? "true" : "false";
  source->AddString("keyboardOverlayHasChromeOSDiamondKey",
                    has_diamond_key_value);
  source->set_json_path("strings.js");
  source->set_use_json_js_format_v2();
  source->add_resource_path("keyboard_overlay.js", IDR_KEYBOARD_OVERLAY_JS);
  source->set_default_resource(IDR_KEYBOARD_OVERLAY_HTML);
  return source;
}

}  // namespace

// The handler for Javascript messages related to the "keyboardoverlay" view.
class KeyboardOverlayHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<KeyboardOverlayHandler> {
 public:
  explicit KeyboardOverlayHandler(Profile* profile);
  virtual ~KeyboardOverlayHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Called when the page requires the input method ID corresponding to the
  // current input method or keyboard layout during initialization.
  void GetInputMethodId(const ListValue* args);

  // Called when the page requres the information of modifier key remapping
  // during the initialization.
  void GetLabelMap(const ListValue* args);

  // Called when the learn more link is clicked.
  void OpenLearnMorePage(const ListValue* args);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// KeyboardOverlayHandler
//
////////////////////////////////////////////////////////////////////////////////
KeyboardOverlayHandler::KeyboardOverlayHandler(Profile* profile)
    : profile_(profile) {
}

KeyboardOverlayHandler::~KeyboardOverlayHandler() {
}

void KeyboardOverlayHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getInputMethodId",
      base::Bind(&KeyboardOverlayHandler::GetInputMethodId,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getLabelMap",
      base::Bind(&KeyboardOverlayHandler::GetLabelMap,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openLearnMorePage",
      base::Bind(&KeyboardOverlayHandler::OpenLearnMorePage,
                 base::Unretained(this)));
}

void KeyboardOverlayHandler::GetInputMethodId(const ListValue* args) {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::GetInputMethodManager();
  const chromeos::input_method::InputMethodDescriptor& descriptor =
      manager->GetCurrentInputMethod();
  StringValue param(descriptor.id());
  web_ui()->CallJavascriptFunction("initKeyboardOverlayId", param);
}

void KeyboardOverlayHandler::GetLabelMap(const ListValue* args) {
  DCHECK(profile_);
  PrefService* pref_service = profile_->GetPrefs();
  typedef std::map<ModifierKey, ModifierKey> ModifierMap;
  ModifierMap modifier_map;
  modifier_map[chromeos::input_method::kSearchKey] = static_cast<ModifierKey>(
      pref_service->GetInteger(prefs::kLanguageRemapSearchKeyTo));
  modifier_map[chromeos::input_method::kControlKey] = static_cast<ModifierKey>(
      pref_service->GetInteger(prefs::kLanguageRemapControlKeyTo));
  modifier_map[chromeos::input_method::kAltKey] = static_cast<ModifierKey>(
      pref_service->GetInteger(prefs::kLanguageRemapAltKeyTo));
  // TODO(mazda): Support prefs::kLanguageRemapCapsLockKeyTo once Caps Lock is
  // added to the overlay UI.

  DictionaryValue dict;
  for (ModifierMap::const_iterator i = modifier_map.begin();
       i != modifier_map.end(); ++i) {
    dict.SetString(ModifierKeyToLabel(i->first), ModifierKeyToLabel(i->second));
  }

  web_ui()->CallJavascriptFunction("initIdentifierMap", dict);
}

void KeyboardOverlayHandler::OpenLearnMorePage(const ListValue* args) {
  web_ui()->GetWebContents()->GetDelegate()->OpenURLFromTab(
      web_ui()->GetWebContents(),
      content::OpenURLParams(GURL(kLearnMoreURL),
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             content::PAGE_TRANSITION_LINK,
                             false));
}

////////////////////////////////////////////////////////////////////////////////
//
// KeyboardOverlayUI
//
////////////////////////////////////////////////////////////////////////////////

KeyboardOverlayUI::KeyboardOverlayUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  KeyboardOverlayHandler* handler = new KeyboardOverlayHandler(profile);
  web_ui->AddMessageHandler(handler);

  // Set up the chrome://keyboardoverlay/ source.
  ChromeURLDataManager::AddDataSourceImpl(
      profile, CreateKeyboardOverlayUIHTMLSource());
}
