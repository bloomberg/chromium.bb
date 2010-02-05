// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookie_modal_dialog.h"

#include "chrome/browser/views/cookie_prompt_view.h"
#include "googleurl/src/gurl.h"

CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    const GURL& url,
    const std::string& cookie_line,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      url_(url),
      cookie_line_(cookie_line),
      cookie_ui_(true),
      delegate_(delegate) {
}


CookiePromptModalDialog::CookiePromptModalDialog(
    TabContents* tab_contents,
    const BrowsingDataLocalStorageHelper::LocalStorageInfo& storage_info,
    CookiePromptModalDialogDelegate* delegate)
    : AppModalDialog(tab_contents, std::wstring()),
      storage_info_(storage_info),
      cookie_ui_(false),
      delegate_(delegate) {
}


