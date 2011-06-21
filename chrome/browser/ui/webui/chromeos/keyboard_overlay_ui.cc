// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/keyboard_overlay_ui.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
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
  { "keyboardOverlayBack", IDS_KEYBOARD_OVERLAY_BACK },
  { "keyboardOverlayBookmarkAllTabs", IDS_KEYBOARD_OVERLAY_BOOKMARK_ALL_TABS },
  { "keyboardOverlayBookmarkCurrentPage",
    IDS_KEYBOARD_OVERLAY_BOOKMARK_CURRENT_PAGE },
  { "keyboardOverlayClearBrowsingDataDialog",
    IDS_KEYBOARD_OVERLAY_CLEAR_BROWSING_DATA_DIALOG },
  { "keyboardOverlayCloseTab", IDS_KEYBOARD_OVERLAY_CLOSE_TAB },
  { "keyboardOverlayCloseWindow", IDS_KEYBOARD_OVERLAY_CLOSE_WINDOW },
  { "keyboardOverlayCloseWindow", IDS_KEYBOARD_OVERLAY_CLOSE_WINDOW },
  { "keyboardOverlayContentBrowser", IDS_KEYBOARD_OVERLAY_CONTENT_BROWSER },
  { "keyboardOverlayCopy", IDS_KEYBOARD_OVERLAY_COPY },
  { "keyboardOverlayCut", IDS_KEYBOARD_OVERLAY_CUT },
  { "keyboardOverlayCycleThroughInputMethods",
    IDS_KEYBOARD_OVERLAY_CYCLE_THROUGH_INPUT_METHODS },
  { "keyboardOverlayDeleteWord", IDS_KEYBOARD_OVERLAY_DELETE_WORD },
  { "keyboardOverlayDeveloperTools", IDS_KEYBOARD_OVERLAY_DEVELOPER_TOOLS },
  { "keyboardOverlayDomInspector", IDS_KEYBOARD_OVERLAY_DOM_INSPECTOR },
  { "keyboardOverlayDownloads", IDS_KEYBOARD_OVERLAY_DOWNLOADS },
  { "keyboardOverlayEnd", IDS_KEYBOARD_OVERLAY_END },
  { "keyboardOverlayFindAgain", IDS_KEYBOARD_OVERLAY_FIND_AGAIN },
  { "keyboardOverlayFindPrevious", IDS_KEYBOARD_OVERLAY_FIND_PREVIOUS },
  { "keyboardOverlayFindText", IDS_KEYBOARD_OVERLAY_FIND_TEXT },
  { "keyboardOverlayFocusAddressBar", IDS_KEYBOARD_OVERLAY_FOCUS_ADDRESS_BAR },
  { "keyboardOverlayFocusAddressBarInSearchMode",
    IDS_KEYBOARD_OVERLAY_FOCUS_ADDRESS_BAR_IN_SEARCH_MODE },
  { "keyboardOverlayForward", IDS_KEYBOARD_OVERLAY_FORWARD },
  { "keyboardOverlayHelp", IDS_KEYBOARD_OVERLAY_HELP },
  { "keyboardOverlayHistory", IDS_KEYBOARD_OVERLAY_HISTORY },
  { "keyboardOverlayHome", IDS_KEYBOARD_OVERLAY_HOME },
  { "keyboardOverlayInputUnicodeCharacters",
    IDS_KEYBOARD_OVERLAY_INPUT_UNICODE_CHARACTERS },
  { "keyboardOverlayLockScreenOrPowerOff",
    IDS_KEYBOARD_OVERLAY_LOCK_SCREEN_OR_POWER_OFF },
  { "keyboardOverlayNewIncognitoWindow",
    IDS_KEYBOARD_OVERLAY_NEW_INCOGNITO_WINDOW },
  { "keyboardOverlayNewTab", IDS_KEYBOARD_OVERLAY_NEW_TAB },
  { "keyboardOverlayNewWindow", IDS_KEYBOARD_OVERLAY_NEW_WINDOW },
  { "keyboardOverlayNextWindow", IDS_KEYBOARD_OVERLAY_NEXT_WINDOW },
  { "keyboardOverlayOpenAddressInNewTab",
    IDS_KEYBOARD_OVERLAY_OPEN_ADDRESS_IN_NEW_TAB },
  { "keyboardOverlayOpenFileManager", IDS_KEYBOARD_OVERLAY_OPEN_FILE_MANAGER },
  { "keyboardOverlayPageDown", IDS_KEYBOARD_OVERLAY_PAGE_DOWN },
  { "keyboardOverlayPageUp", IDS_KEYBOARD_OVERLAY_PAGE_UP },
  { "keyboardOverlayPaste", IDS_KEYBOARD_OVERLAY_PASTE },
  { "keyboardOverlayPasteAsPlainText",
    IDS_KEYBOARD_OVERLAY_PASTE_AS_PLAIN_TEXT },
  { "keyboardOverlayPreviousWindow", IDS_KEYBOARD_OVERLAY_PREVIOUS_WINDOW },
  { "keyboardOverlayPrint", IDS_KEYBOARD_OVERLAY_PRINT },
  { "keyboardOverlayReloadCurrentPage",
    IDS_KEYBOARD_OVERLAY_RELOAD_CURRENT_PAGE },
  { "keyboardOverlayReloadIgnoringCache",
    IDS_KEYBOARD_OVERLAY_RELOAD_IGNORING_CACHE },
  { "keyboardOverlayReopenLastClosedTab",
    IDS_KEYBOARD_OVERLAY_REOPEN_LAST_CLOSED_TAB },
  { "keyboardOverlayResetZoom", IDS_KEYBOARD_OVERLAY_RESET_ZOOM },
  { "keyboardOverlaySave", IDS_KEYBOARD_OVERLAY_SAVE },
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
  { "keyboardOverlayToggleBookmarkBar",
    IDS_KEYBOARD_OVERLAY_TOGGLE_BOOKMARK_BAR },
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
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

 private:
  // Called when the page requires the keyboard overaly ID corresponding to the
  // current input method or keyboard layout during initialization.
  void GetKeyboardOverlayId(const ListValue* args);

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
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      keyboard_overlay_html, &localized_strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
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
  web_ui_->RegisterMessageCallback("getKeyboardOverlayId",
      NewCallback(this, &KeyboardOverlayHandler::GetKeyboardOverlayId));
  web_ui_->RegisterMessageCallback("getLabelMap",
      NewCallback(this, &KeyboardOverlayHandler::GetLabelMap));
}

void KeyboardOverlayHandler::GetKeyboardOverlayId(const ListValue* args) {
  chromeos::InputMethodLibrary* library =
      chromeos::CrosLibrary::Get()->GetInputMethodLibrary();
  const chromeos::input_method::InputMethodDescriptor& descriptor =
      library->current_input_method();
  const std::string keyboard_overlay_id =
      library->GetKeyboardOverlayId(descriptor.id);
  StringValue param(keyboard_overlay_id);
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
  KeyboardOverlayHandler* handler =
      new KeyboardOverlayHandler(contents->profile());
  AddMessageHandler((handler)->Attach(this));
  KeyboardOverlayUIHTMLSource* html_source = new KeyboardOverlayUIHTMLSource();

  // Set up the chrome://keyboardoverlay/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}
