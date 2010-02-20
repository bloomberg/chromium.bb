// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COOKIE_PROMPT_MODAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_COOKIE_PROMPT_MODAL_DIALOG_DELEGATE_H_

// Delegate for handling modal dialog results from CookiePromptModalDialog.
// The implementer of this MUST guarentee that from the time it's passed to the
// CookieModalDialog until one of these methods are called it will not be
// deleted.
class CookiePromptModalDialogDelegate {
 public:
  // Allow site data to be set.
  virtual void AllowSiteData(bool session_expire) = 0;

  // Block site data from being stored.
  virtual void BlockSiteData() = 0;

 protected:
  virtual ~CookiePromptModalDialogDelegate() {}
};

#endif  // CHROME_BROWSER_COOKIE_PROMPT_MODAL_DIALOG_DELEGATE_H_

