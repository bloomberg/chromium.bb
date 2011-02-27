// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/webui/keyboard_overlay_ui.h"

#include "base/callback.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "third_party/cros/chromeos_input_method.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"


class KeyboardOverlayUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  KeyboardOverlayUIHTMLSource();

  // Called when the keyboard overlay has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
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
  KeyboardOverlayHandler();
  virtual ~KeyboardOverlayHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

 private:
  // Called when the page requires the keyboard overaly ID corresponding to the
  // current input method or keyboard layout during initialization.
  void GetKeyboardOverlayId(const ListValue* args);

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
                                              bool is_off_the_record,
                                              int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString("keyboardOverlayTitle",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_TITLE));
  localized_strings.SetString("keyboardOverlayInstructions",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_INSTRUCTIONS));
  localized_strings.SetString("keyboardOverlayInstructionsHide",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_INSTRUCTIONS_HIDE));
  localized_strings.SetString("keyboardOverlayActivateLastTab",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_LAST_TAB));
  localized_strings.SetString("keyboardOverlayActivateNextTab",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_NEXT_TAB));
  localized_strings.SetString("keyboardOverlayActivatePreviousTab",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_PREVIOUS_TAB));
  localized_strings.SetString("keyboardOverlayActivateTab1",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_1));
  localized_strings.SetString("keyboardOverlayActivateTab2",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_2));
  localized_strings.SetString("keyboardOverlayActivateTab3",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_3));
  localized_strings.SetString("keyboardOverlayActivateTab4",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_4));
  localized_strings.SetString("keyboardOverlayActivateTab5",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_5));
  localized_strings.SetString("keyboardOverlayActivateTab6",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_6));
  localized_strings.SetString("keyboardOverlayActivateTab7",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_7));
  localized_strings.SetString("keyboardOverlayActivateTab8",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ACTIVATE_TAB_8));
  localized_strings.SetString("keyboardOverlayAddWwwAndComAndOpenAddress",
      l10n_util::GetStringUTF16(
          IDS_KEYBOARD_OVERLAY_ADD_WWW_AND_COM_AND_OPEN_ADDRESS));
  localized_strings.SetString("keyboardOverlayBookmarkCurrentPage",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_BOOKMARK_CURRENT_PAGE));
  localized_strings.SetString("keyboardOverlayBookmarkAllTabs",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_BOOKMARK_ALL_TABS));
  localized_strings.SetString("keyboardOverlayCloseTab",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_CLOSE_TAB));
  localized_strings.SetString("keyboardOverlayCloseWindow",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_CLOSE_WINDOW));
  localized_strings.SetString("keyboardOverlayDeleteWord",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_DELETE_WORD));
  localized_strings.SetString("keyboardOverlayDeveloperTools",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_DEVELOPER_TOOLS));
  localized_strings.SetString("keyboardOverlayFindAgain",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_FIND_AGAIN));
  localized_strings.SetString("keyboardOverlayFindPrevious",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_FIND_PREVIOUS));
  localized_strings.SetString("keyboardOverlayFindPrevious",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_FIND_PREVIOUS));
  localized_strings.SetString("keyboardOverlayFindText",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_FIND_TEXT));
  localized_strings.SetString("keyboardOverlayFocusAddressBar",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_FOCUS_ADDRESS_BAR));
  localized_strings.SetString("keyboardOverlayFocusAddressBar",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_FOCUS_ADDRESS_BAR));
  localized_strings.SetString("keyboardOverlayFocusAddressBarInSearchMode",
      l10n_util::GetStringUTF16(
          IDS_KEYBOARD_OVERLAY_FOCUS_ADDRESS_BAR_IN_SEARCH_MODE));
  localized_strings.SetString("keyboardOverlayDomInspector",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_DOM_INSPECTOR));
  localized_strings.SetString("keyboardOverlayDownloads",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_DOWNLOADS));
  localized_strings.SetString("keyboardOverlayTaskManager",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_TASK_MANAGER));
  localized_strings.SetString("keyboardOverlayBack",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_BACK));
  localized_strings.SetString("keyboardOverlayForward",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_FORWARD));
  localized_strings.SetString("keyboardOverlayForward",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_FORWARD));
  localized_strings.SetString("keyboardOverlayHistory",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_HISTORY));
  localized_strings.SetString("keyboardOverlayNewTab",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_NEW_TAB));
  localized_strings.SetString("keyboardOverlayOpenAddressInNewTab",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_OPEN_ADDRESS_IN_NEW_TAB));
  localized_strings.SetString("keyboardOverlayNewIncognitoWindow",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_NEW_INCOGNITO_WINDOW));
  localized_strings.SetString("keyboardOverlayNewWindow",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_NEW_WINDOW));
  localized_strings.SetString("keyboardOverlayPasteAsPlainText",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_PASTE_AS_PLAIN_TEXT));
  localized_strings.SetString("keyboardOverlayPrint",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_PRINT));
  localized_strings.SetString("keyboardOverlayReloadCurrentPage",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_RELOAD_CURRENT_PAGE));
  localized_strings.SetString("keyboardOverlayReopenLastClosedTab",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_REOPEN_LAST_CLOSED_TAB));
  localized_strings.SetString("keyboardOverlayResetZoom",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_RESET_ZOOM));
  localized_strings.SetString("keyboardOverlaySelectWordAtATime",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_SELECT_WORD_AT_A_TIME));
  localized_strings.SetString("keyboardOverlayToggleBookmarkBar",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_TOGGLE_BOOKMARK_BAR));
  localized_strings.SetString("keyboardOverlayViewSource",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_VIEW_SOURCE));
  localized_strings.SetString("keyboardOverlayZoomIn",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ZOOM_IN));
  localized_strings.SetString("keyboardOverlayZoomOut",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_ZOOM_OUT));
  localized_strings.SetString("keyboardOverlayResetZoom",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_RESET_ZOOM));
  localized_strings.SetString("keyboardOverlayFocusAddressBarInSearchMode",
      l10n_util::GetStringUTF16(
          IDS_KEYBOARD_OVERLAY_FOCUS_ADDRESS_BAR_IN_SEARCH_MODE));
  localized_strings.SetString("keyboardOverlayFullScreen",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_FULL_SCREEN));
  localized_strings.SetString("keyboardOverlayTakeScreenshot",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_TAKE_SCREENSHOT));
  localized_strings.SetString("keyboardOverlayHome",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_HOME));
  localized_strings.SetString("keyboardOverlayEnd",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_END));
  localized_strings.SetString("keyboardOverlayNextWindow",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_NEXT_WINDOW));
  localized_strings.SetString("keyboardOverlayContentBrowser",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_CONTENT_BROWSER));
  localized_strings.SetString("keyboardOverlayPageUp",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_PAGE_UP));
  localized_strings.SetString("keyboardOverlayPageDown",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_PAGE_DOWN));
  localized_strings.SetString("keyboardOverlayPreviousWindow",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_PREVIOUS_WINDOW));
  localized_strings.SetString("keyboardOverlayUseExternalMonitor",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_USE_EXTERNAL_MONITOR));
  localized_strings.SetString("keyboardOverlayReloadIgnoringCache",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_RELOAD_IGNORING_CACHE));
  localized_strings.SetString("keyboardOverlaySave",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_SAVE));
  localized_strings.SetString("keyboardOverlayScrollUpOnePage",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_SCROLL_UP_ONE_PAGE));
  localized_strings.SetString("keyboardOverlaySettings",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_SETTINGS));
  localized_strings.SetString("keyboardOverlaySignOut",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_SIGN_OUT));
  localized_strings.SetString("keyboardOverlayUndo",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_UNDO));
  localized_strings.SetString("keyboardOverlayWordMove",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_WORD_MOVE));
  localized_strings.SetString("keyboardOverlaySelectAll",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_SELECT_ALL));
  localized_strings.SetString("keyboardOverlaySelectPreviousInputMethod",
      l10n_util::GetStringUTF16(
          IDS_KEYBOARD_OVERLAY_SELECT_PREVIOUS_INPUT_METHOD));
  localized_strings.SetString("keyboardOverlayCycleThroughInputMethods",
      l10n_util::GetStringUTF16(
          IDS_KEYBOARD_OVERLAY_CYCLE_THROUGH_INPUT_METHODS));
  localized_strings.SetString("keyboardOverlayCloseWindow",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_CLOSE_WINDOW));
  localized_strings.SetString("keyboardOverlayViewKeyboardOverlay",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_VIEW_KEYBOARD_OVERLAY));
  localized_strings.SetString("keyboardOverlayCut",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_CUT));
  localized_strings.SetString("keyboardOverlayCopy",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_COPY));
  localized_strings.SetString("keyboardOverlayPaste",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_PASTE));
  localized_strings.SetString("keyboardOverlayHelp",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_OVERLAY_HELP));

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
KeyboardOverlayHandler::KeyboardOverlayHandler() {
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
}

void KeyboardOverlayHandler::GetKeyboardOverlayId(const ListValue* args) {
  chromeos::InputMethodLibrary* library =
      chromeos::CrosLibrary::Get()->GetInputMethodLibrary();
  const chromeos::InputMethodDescriptor& descriptor =
      library->current_input_method();
  const std::string keyboard_overlay_id =
      library->GetKeyboardOverlayId(descriptor.id);
  StringValue param(keyboard_overlay_id);
  web_ui_->CallJavascriptFunction(L"initKeyboardOverlayId", param);
}

////////////////////////////////////////////////////////////////////////////////
//
// KeyboardOverlayUI
//
////////////////////////////////////////////////////////////////////////////////

KeyboardOverlayUI::KeyboardOverlayUI(TabContents* contents)
    : HtmlDialogUI(contents) {
  KeyboardOverlayHandler* handler = new KeyboardOverlayHandler();
  AddMessageHandler((handler)->Attach(this));
  KeyboardOverlayUIHTMLSource* html_source = new KeyboardOverlayUIHTMLSource();

  // Set up the chrome://keyboardoverlay/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}
