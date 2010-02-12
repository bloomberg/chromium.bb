// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MESSAGE_BOX_HANDLER_H_
#define CHROME_BROWSER_MESSAGE_BOX_HANDLER_H_

#include <string>

#include "base/string16.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "net/base/cookie_monster.h"

class CookiePromptModalDialogDelegate;
class GURL;
class JavaScriptMessageBoxClient;
class TabContents;

// Creates and runs a Javascript Message Box dialog.
// The dialog type is specified within |dialog_flags|, the
// default static display text is in |message_text| and if the dialog box is
// a user input prompt() box, the default text for the text field is in
// |default_prompt_text|. The result of the operation is returned using
// |reply_msg|.
void RunJavascriptMessageBox(JavaScriptMessageBoxClient* client,
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

// TODO(zelidrag): bug 32719, implement these modal dialogs on Linux and Mac.
#if defined(OS_WIN)
// This will display a modal dialog box with cookie information asking
// user to accept or reject the cookie. The caller should pass |delegate|
// that will handle the reply from the dialog.
void RunCookiePrompt(TabContents* tab_contents,
                     const GURL& origin,
                     const std::string& cookie_line,
                     CookiePromptModalDialogDelegate* delegate);

// This will display a modal dialog box with local storage information asking
// user to accept or reject it. The caller should pass |delegate|
// that will handle the reply from the dialog.
void RunLocalStoragePrompt(
    TabContents* tab_contents,
    const GURL& origin,
    const string16& key,
    const string16& value,
    CookiePromptModalDialogDelegate* delegate);
#endif

#endif  // CHROME_BROWSER_MESSAGE_BOX_HANDLER_H_

