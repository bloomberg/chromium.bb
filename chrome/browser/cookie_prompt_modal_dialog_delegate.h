// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COOKIE_PROMPT_MODAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_COOKIE_PROMPT_MODAL_DIALOG_DELEGATE_H_

// Delegate for handling modal dialog results from CookiePromptModalDialog.
class CookiePromptModalDialogDelegate {
 public:
  // Allow site data to be set. If |remember| is true, record this decision
  // for this host.
  virtual void AllowSiteData(bool remember, bool session_expire) = 0;

  // Block site data from being stored. If |remember| is true, record this
  // decision for this host.
  virtual void BlockSiteData(bool remember) = 0;

 protected:
  virtual ~CookiePromptModalDialogDelegate() {}
};

#endif  // CHROME_BROWSER_COOKIE_PROMPT_MODAL_DIALOG_DELEGATE_H_

