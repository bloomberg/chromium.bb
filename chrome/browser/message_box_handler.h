// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MESSAGE_BOX_HANDLER_H_
#define CHROME_BROWSER_MESSAGE_BOX_HANDLER_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "net/base/cookie_monster.h"

class CookiePromptModalDialogDelegate;
class GURL;
class HostContentSettingsMap;
class JavaScriptMessageBoxClient;
class TabContents;
class Profile;

// Creates and runs a Javascript Message Box dialog.
// The dialog type is specified within |dialog_flags|, the
// default static display text is in |message_text| and if the dialog box is
// a user input prompt() box, the default text for the text field is in
// |default_prompt_text|. The result of the operation is returned using
// |reply_msg|.
void RunJavascriptMessageBox(Profile* profile,
                             JavaScriptMessageBoxClient* client,
                             const GURL& frame_url,
                             int dialog_flags,
                             const std::wstring& message_text,
                             const std::wstring& default_prompt_text,
                             bool display_suppress_checkbox,
                             IPC::Message* reply_msg);

// This will display a modal dialog box with a header and footer asking the
// the user if they wish to navigate away from a page, with additional text
// |message_text| between the header and footer. The users response is
// returned to the renderer using |reply_msg|.
void RunBeforeUnloadDialog(TabContents* tab_contents,
                           const std::wstring& message_text,
                           IPC::Message* reply_msg);

// This will display a modal dialog box with cookie information asking
// user to accept or reject the cookie. The caller should pass |delegate|
// that will handle the reply from the dialog.
void RunCookiePrompt(TabContents* tab_contents,
                     HostContentSettingsMap* host_content_settings_map,
                     const GURL& origin,
                     const std::string& cookie_line,
                     CookiePromptModalDialogDelegate* delegate);

// This will display a modal dialog box with local storage information asking
// user to accept or reject it. The caller should pass |delegate|
// that will handle the reply from the dialog.
void RunLocalStoragePrompt(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& origin,
    const string16& key,
    const string16& value,
    CookiePromptModalDialogDelegate* delegate);

// This will display a modal dialog box with the database name on every open
// and ask the user to accept or reject it. The caller should pass |delegate|
// that will handle the reply from the dialog.
void RunDatabasePrompt(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& origin,
    const string16& database_name,
    const string16& display_name,
    unsigned long estimated_size,
    CookiePromptModalDialogDelegate* delegate);

// This will display a modal dialog box with the |manifest_url| and ask the
// user to accept or reject it. The caller should pass |delegate| that will
// handle the reply from the dialog.
void RunAppCachePrompt(
    TabContents* tab_contents,
    HostContentSettingsMap* host_content_settings_map,
    const GURL& manifest_url,
    CookiePromptModalDialogDelegate* delegate);

#endif  // CHROME_BROWSER_MESSAGE_BOX_HANDLER_H_
