// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_JSON_REQUESTS_H_
#define CHROME_TEST_AUTOMATION_AUTOMATION_JSON_REQUESTS_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/common/automation_constants.h"
#include "ui/base/keycodes/keyboard_codes.h"

class AutomationMessageSender;
class FilePath;
class GURL;
class DictionaryValue;
class ListValue;
class Value;

struct WebKeyEvent {
  WebKeyEvent(automation::KeyEventTypes type,
              ui::KeyboardCode key_code,
              const std::string& unmodified_text,
              const std::string& modified_text,
              int modifiers);

  automation::KeyEventTypes type;
  ui::KeyboardCode key_code;
  std::string unmodified_text;
  std::string modified_text;
  int modifiers;
};

// Sends a JSON request to the chrome automation provider. Returns true
// if the JSON request was successfully sent and the reply was received.
// If true, |success| will be set to whether the JSON request was
// completed successfully by the automation provider.
bool SendAutomationJSONRequest(AutomationMessageSender* sender,
                               const std::string& request,
                               int timeout_ms,
                               std::string* reply,
                               bool* success) WARN_UNUSED_RESULT;

// Requests the current browser and tab indices for the given tab ID.
// Returns true on success.
bool SendGetIndicesFromTabIdJSONRequest(
    AutomationMessageSender* sender,
    int tab_id,
    int* browser_index,
    int* tab_index,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests the current browser and tab indices for the given |TabProxy|
// handle. Returns true on success.
bool SendGetIndicesFromTabHandleJSONRequest(
    AutomationMessageSender* sender,
    int tab_proxy_handle,
    int* browser_index,
    int* tab_index,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to navigate to the given url and wait for the given number of
// navigations to complete. Returns true on success.
bool SendNavigateToURLJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const GURL& url,
    int navigation_count,
    AutomationMsg_NavigationResponseValues* nav_response,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests the given javascript to be executed in the frame specified by the
// given xpath. Returns true on success. If true, |result| will be set to the
// result of the execution and ownership will be given to the caller.
bool SendExecuteJavascriptJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const std::string& frame_xpath,
    const std::string& javascript,
    Value** result,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests the specified tab to go forward. Waits for the load to complete.
// Returns true on success.
bool SendGoForwardJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests the specified tab to go back. Waits for the load to complete.
// Returns true on success.
bool SendGoBackJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests the specified tab to reload. Waits for the load to complete.
// Returns true on success.
bool SendReloadJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests a snapshot of the entire page to be saved to the given path
// in PNG format.
// Returns true on success.
bool SendCaptureEntirePageJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const FilePath& path,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests the url of the specified tab. Returns true on success.
bool SendGetTabURLJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* url,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests the title of the specified tab. Returns true on success.
bool SendGetTabTitleJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* tab_title,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests all the cookies for the given URL. On success returns true and
// caller takes ownership of |cookies|, which is a list of all the cookies in
// dictionary format.
bool SendGetCookiesJSONRequest(
    AutomationMessageSender* sender,
    const std::string& url,
    ListValue** cookies,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests all the cookies for the given URL. Returns true on success.
// Use |SendGetCookiesJSONRequest| for chrome versions greater than 11.
// TODO(kkania): Remove this function when version 12 is stable.
bool SendGetCookiesJSONRequestDeprecated(
    AutomationMessageSender* sender,
    int browser_index,
    const std::string& url,
    std::string* cookies) WARN_UNUSED_RESULT;

// Requests deletion of the cookie with the given name and URL. Returns true
// on success.
bool SendDeleteCookieJSONRequest(
    AutomationMessageSender* sender,
    const std::string& url,
    const std::string& cookie_name,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests deletion of the cookie with the given name and URL. Returns true
// on success. Use |SendDeleteCookieJSONRequest| for chrome versions greater
// than 11.
// TODO(kkania): Remove this function when version 12 is stable.
bool SendDeleteCookieJSONRequestDeprecated(
    AutomationMessageSender* sender,
    int browser_index,
    const std::string& url,
    const std::string& cookie_name) WARN_UNUSED_RESULT;

// Requests setting the given cookie for the given URL. Returns true on
// success. The caller retains ownership of |cookie_dict|.
bool SendSetCookieJSONRequest(
    AutomationMessageSender* sender,
    const std::string& url,
    DictionaryValue* cookie_dict,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests setting the given cookie for the given URL. Returns true on
// success. Use |SendSetCookieJSONRequest| instead for chrome versions greater
// than 11.
// TODO(kkania): Remove this when version 12 is stable.
bool SendSetCookieJSONRequestDeprecated(
    AutomationMessageSender* sender,
    int browser_index,
    const std::string& url,
    const std::string& cookie) WARN_UNUSED_RESULT;

// Requests the IDs for all open tabs. Returns true on success.
bool SendGetTabIdsJSONRequest(
    AutomationMessageSender* sender,
    std::vector<int>* tab_ids,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests whether the given tab ID is valid. Returns true on success.
bool SendIsTabIdValidJSONRequest(
    AutomationMessageSender* sender,
    int tab_id,
    bool* is_valid,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to close the given tab. Returns true on success.
bool SendCloseTabJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to send the WebKit event for a mouse move to the given
// coordinate in the specified tab. Returns true on success.
bool SendMouseMoveJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int x,
    int y,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to send the WebKit events for a mouse click at the given
// coordinate in the specified tab. Returns true on success.
bool SendMouseClickJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    automation::MouseButton button,
    int x,
    int y,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to send the WebKit events for a mouse drag from the start to end
// coordinates given in the specified tab. Returns true on success.
bool SendMouseDragJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to send the WebKit event for a mouse button down at the given
// coordinate in the specified tab. Returns true on success.
bool SendMouseButtonDownJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int x,
    int y,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to send the WebKit event for a mouse button up at the given
// coordinate in the specified tab. Returns true on success.
bool SendMouseButtonUpJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int x,
    int y,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to send the WebKit event for a mouse double click at the given
// coordinate in the specified tab. Returns true on success.
bool SendMouseDoubleClickJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    int x,
    int y,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to send the WebKit event for the given |WebKeyEvent| in a
// specified tab. Returns true on success.
bool SendWebKeyEventJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    const WebKeyEvent& key_event,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to send the key event for the given keycode+modifiers to a
// browser window containing the specified tab. Returns true on success.
bool SendNativeKeyEventJSONRequest(
    AutomationMessageSender* sender,
    int browser_index,
    int tab_index,
    ui::KeyboardCode key_code,
    int modifiers,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to get the active JavaScript modal dialog's message. Returns true
// on success.
bool SendGetAppModalDialogMessageJSONRequest(
    AutomationMessageSender* sender,
    std::string* message,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to accept or dismiss the active JavaScript modal dialog.
// Returns true on success.
bool SendAcceptOrDismissAppModalDialogJSONRequest(
    AutomationMessageSender* sender,
    bool accept,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to accept the active JavaScript modal dialog with the given prompt
// text. Returns true on success.
bool SendAcceptPromptAppModalDialogJSONRequest(
    AutomationMessageSender* sender,
    const std::string& prompt_text,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests to wait for all tabs to stop loading. Returns true on success.
bool SendWaitForAllTabsToStopLoadingJSONRequest(
    AutomationMessageSender* sender,
    std::string* error_msg) WARN_UNUSED_RESULT;

// Requests the version of ChromeDriver automation supported by the automation
// server. Returns true on success.
bool SendGetChromeDriverAutomationVersion(
    AutomationMessageSender* sender,
    int* version,
    std::string* error_msg) WARN_UNUSED_RESULT;

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_JSON_REQUESTS_H_
