// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/keyboard_overlay_ui.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using chromeos::input_method::ModifierKey;

namespace {

struct ModifierToLabel {
  const ModifierKey modifier;
  const char* label;
} kModifierToLabels[] = {
  {chromeos::input_method::kSearchKey, "search"},
  {chromeos::input_method::kLeftControlKey, "ctrl"},
  {chromeos::input_method::kLeftAltKey, "alt"},
  {chromeos::input_method::kVoidKey, "disabled"},
  {chromeos::input_method::kCapsLockKey, "caps lock"},
};

struct I18nContentToMessage {
  const char* i18n_content;
  int message;
} kI18nContentToMessage[] = {
  { "keyboardOverlayTitle", IDS_KEYBOARD_OVERLAY_TITLE },
  { "keyboardOverlayInstructions", IDS_KEYBOARD_OVERLAY_INSTRUCTIONS },
  { "keyboardOverlayInstructionsHide", IDS_KEYBOARD_OVERLAY_INSTRUCTIONS_HIDE },
  { "keyboardOverlayActivateLastTab", IDS_KEYBOARD_OVERLAY_ACTIVATE_LAST_TAB },
  { "keyboardOverlayActivateLastWindow",
    IDS_KEYBOARD_OVERLAY_ACTIVATE_LAST_WINDOW },
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
  { "keyboardOverlayActivateWindow1", IDS_KEYBOARD_OVERLAY_ACTIVATE_WINDOW_1 },
  { "keyboardOverlayActivateWindow2", IDS_KEYBOARD_OVERLAY_ACTIVATE_WINDOW_2 },
  { "keyboardOverlayActivateWindow3", IDS_KEYBOARD_OVERLAY_ACTIVATE_WINDOW_3 },
  { "keyboardOverlayActivateWindow4", IDS_KEYBOARD_OVERLAY_ACTIVATE_WINDOW_4 },
  { "keyboardOverlayActivateWindow5", IDS_KEYBOARD_OVERLAY_ACTIVATE_WINDOW_5 },
  { "keyboardOverlayActivateWindow6", IDS_KEYBOARD_OVERLAY_ACTIVATE_WINDOW_6 },
  { "keyboardOverlayActivateWindow7", IDS_KEYBOARD_OVERLAY_ACTIVATE_WINDOW_7 },
  { "keyboardOverlayActivateWindow8", IDS_KEYBOARD_OVERLAY_ACTIVATE_WINDOW_8 },
  { "keyboardOverlayAddWwwAndComAndOpenAddress",
    IDS_KEYBOARD_OVERLAY_ADD_WWW_AND_COM_AND_OPEN_ADDRESS },
  { "keyboardOverlayBookmarkAllTabs", IDS_KEYBOARD_OVERLAY_BOOKMARK_ALL_TABS },
  { "keyboardOverlayBookmarkCurrentPage",
    IDS_KEYBOARD_OVERLAY_BOOKMARK_CURRENT_PAGE },
  { "keyboardOverlayBookmarkManager", IDS_KEYBOARD_OVERLAY_BOOKMARK_MANAGER },
  { "keyboardOverlayClearBrowsingDataDialog",
    IDS_KEYBOARD_OVERLAY_CLEAR_BROWSING_DATA_DIALOG },
  { "keyboardOverlayCloseTab", IDS_KEYBOARD_OVERLAY_CLOSE_TAB },
  { "keyboardOverlayCloseWindow", IDS_KEYBOARD_OVERLAY_CLOSE_WINDOW },
  { "keyboardOverlayContentBrowser", IDS_KEYBOARD_OVERLAY_CONTENT_BROWSER },
  { "keyboardOverlayCopy", IDS_KEYBOARD_OVERLAY_COPY },
  { "keyboardOverlayCut", IDS_KEYBOARD_OVERLAY_CUT },
  { "keyboardOverlayCycleThroughInputMethods",
    IDS_KEYBOARD_OVERLAY_CYCLE_THROUGH_INPUT_METHODS },
  { "keyboardOverlayDelete", IDS_KEYBOARD_OVERLAY_DELETE },
  { "keyboardOverlayDeleteWord", IDS_KEYBOARD_OVERLAY_DELETE_WORD },
  { "keyboardOverlayDeveloperTools", IDS_KEYBOARD_OVERLAY_DEVELOPER_TOOLS },
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
  { "keyboardOverlayFocusNextPane", IDS_KEYBOARD_OVERLAY_FOCUS_NEXT_PANE },
  { "keyboardOverlayFocusPreviousPane",
    IDS_KEYBOARD_OVERLAY_FOCUS_PREVIOUS_PANE },
  { "keyboardOverlayFocusStatusIcon", IDS_KEYBOARD_OVERLAY_FOCUS_STATUS_ICON },
  { "keyboardOverlayFocusToolbar", IDS_KEYBOARD_OVERLAY_FOCUS_TOOLBAR },
  { "keyboardOverlayFocusWrenchMenu", IDS_KEYBOARD_OVERLAY_FOCUS_WRENCH_MENU },
  { "keyboardOverlayGoBack", IDS_KEYBOARD_OVERLAY_GO_BACK },
  { "keyboardOverlayGoForward", IDS_KEYBOARD_OVERLAY_GO_FORWARD },
  { "keyboardOverlayHelp", IDS_KEYBOARD_OVERLAY_HELP },
  { "keyboardOverlayHistory", IDS_KEYBOARD_OVERLAY_HISTORY },
  { "keyboardOverlayHome", IDS_KEYBOARD_OVERLAY_HOME },
  { "keyboardOverlayInputUnicodeCharacters",
    IDS_KEYBOARD_OVERLAY_INPUT_UNICODE_CHARACTERS },
  { "keyboardOverlayJavascriptConsole",
    IDS_KEYBOARD_OVERLAY_JAVASCRIPT_CONSOLE },
  { "keyboardOverlayLockScreenOrPowerOff",
    IDS_KEYBOARD_OVERLAY_LOCK_SCREEN_OR_POWER_OFF },
  { "keyboardOverlayNewIncognitoWindow",
    IDS_KEYBOARD_OVERLAY_NEW_INCOGNITO_WINDOW },
  { "keyboardOverlayNewTab", IDS_KEYBOARD_OVERLAY_NEW_TAB },
  { "keyboardOverlayNewWindow", IDS_KEYBOARD_OVERLAY_NEW_WINDOW },
  { "keyboardOverlayNextWindow", IDS_KEYBOARD_OVERLAY_NEXT_WINDOW },
  { "keyboardOverlayNextWord", IDS_KEYBOARD_OVERLAY_NEXT_WORD },
  { "keyboardOverlayOpenAddressInNewTab",
    IDS_KEYBOARD_OVERLAY_OPEN_ADDRESS_IN_NEW_TAB },
  { "keyboardOverlayOpenFileManager", IDS_KEYBOARD_OVERLAY_OPEN_FILE_MANAGER },
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
  { "keyboardOverlayResetZoom", IDS_KEYBOARD_OVERLAY_RESET_ZOOM },
  { "keyboardOverlayResizeLeft", IDS_KEYBOARD_OVERLAY_RESIZE_LEFT },
  { "keyboardOverlayResizeRight", IDS_KEYBOARD_OVERLAY_RESIZE_RIGHT },
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
  { "keyboardOverlayShowWrenchMenu", IDS_KEYBOARD_OVERLAY_SHOW_WRENCH_MENU },
  { "keyboardOverlaySignOut", IDS_KEYBOARD_OVERLAY_SIGN_OUT },
  { "keyboardOverlayTakeScreenshot", IDS_KEYBOARD_OVERLAY_TAKE_SCREENSHOT },
  { "keyboardOverlayTaskManager", IDS_KEYBOARD_OVERLAY_TASK_MANAGER },
  { "keyboardOverlayToggleAccessibilityFeatures",
    IDS_KEYBOARD_OVERLAY_TOGGLE_ACCESSIBILITY_FEATURES },
  { "keyboardOverlayToggleBookmarkBar",
    IDS_KEYBOARD_OVERLAY_TOGGLE_BOOKMARK_BAR },
  { "keyboardOverlayToggleSpeechInput",
    IDS_KEYBOARD_OVERLAY_TOGGLE_SPEECH_INPUT },
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

}  // namespace

class KeyboardOverlayUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  KeyboardOverlayUIHTMLSource();

  // Called when the keyboard overlay has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~KeyboardOverlayUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayUIHTMLSource);
};


// The handler for Javascript messages related to the "keyboardoverlay" view.
class KeyboardOverlayHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<KeyboardOverlayHandler> {
 public:
  explicit KeyboardOverlayHandler(Profile* profile);
  virtual ~KeyboardOverlayHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Called when the page requires the input method ID corresponding to the
  // current input method or keyboard layout during initialization.
  void GetInputMethodId(const ListValue* args);

  // Called when the page requres the information of modifier key remapping
  // during the initialization.
  void GetLabelMap(const ListValue* args);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// KeyboardOverlayUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

KeyboardOverlayUIHTMLSource::KeyboardOverlayUIHTMLSource()
    : DataSource(chrome::kChromeUIKeyboardOverlayHost,
                 MessageLoop::current()) {
}

void KeyboardOverlayUIHTMLSource::StartDataRequest(const std::string& path,
                                                   bool is_incognito,
                                                   int request_id) {
  DictionaryValue localized_strings;
  for (size_t i = 0; i < arraysize(kI18nContentToMessage); ++i) {
    localized_strings.SetString(
        kI18nContentToMessage[i].i18n_content,
        l10n_util::GetStringUTF16(kI18nContentToMessage[i].message));
  }

  static const base::StringPiece keyboard_overlay_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_KEYBOARD_OVERLAY_HTML));
  std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      keyboard_overlay_html, &localized_strings);

  SendResponse(request_id, base::RefCountedString::TakeString(&full_html));
}

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

WebUIMessageHandler* KeyboardOverlayHandler::Attach(WebUI* web_ui) {
  return WebUIMessageHandler::Attach(web_ui);
}

void KeyboardOverlayHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("getInputMethodId",
      NewCallback(this, &KeyboardOverlayHandler::GetInputMethodId));
  web_ui_->RegisterMessageCallback("getLabelMap",
      NewCallback(this, &KeyboardOverlayHandler::GetLabelMap));
}

void KeyboardOverlayHandler::GetInputMethodId(const ListValue* args) {
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();
  const chromeos::input_method::InputMethodDescriptor& descriptor =
      manager->current_input_method();
  StringValue param(descriptor.id());
  web_ui_->CallJavascriptFunction("initKeyboardOverlayId", param);
}

void KeyboardOverlayHandler::GetLabelMap(const ListValue* args) {
  DCHECK(profile_);
  PrefService* pref_service = profile_->GetPrefs();
  typedef std::map<ModifierKey, ModifierKey> ModifierMap;
  ModifierMap modifier_map;
  modifier_map[chromeos::input_method::kSearchKey] = static_cast<ModifierKey>(
      pref_service->GetInteger(prefs::kLanguageXkbRemapSearchKeyTo));
  modifier_map[chromeos::input_method::kLeftControlKey] =
      static_cast<ModifierKey>(
          pref_service->GetInteger(prefs::kLanguageXkbRemapControlKeyTo));
  modifier_map[chromeos::input_method::kLeftAltKey] = static_cast<ModifierKey>(
      pref_service->GetInteger(prefs::kLanguageXkbRemapAltKeyTo));

  DictionaryValue dict;
  for (ModifierMap::const_iterator i = modifier_map.begin();
       i != modifier_map.end(); ++i) {
    dict.SetString(ModifierKeyToLabel(i->first), ModifierKeyToLabel(i->second));
  }

  web_ui_->CallJavascriptFunction("initIdentifierMap", dict);
}

////////////////////////////////////////////////////////////////////////////////
//
// KeyboardOverlayUI
//
////////////////////////////////////////////////////////////////////////////////

KeyboardOverlayUI::KeyboardOverlayUI(TabContents* contents)
    : HtmlDialogUI(contents) {
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  KeyboardOverlayHandler* handler =
      new KeyboardOverlayHandler(profile);
  AddMessageHandler((handler)->Attach(this));
  KeyboardOverlayUIHTMLSource* html_source = new KeyboardOverlayUIHTMLSource();

  // Set up the chrome://keyboardoverlay/ source.
  profile->GetChromeURLDataManager()->AddDataSource(html_source);
}
