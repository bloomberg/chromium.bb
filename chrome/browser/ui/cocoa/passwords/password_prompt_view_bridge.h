// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORD_PROMPT_VIEW_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORD_PROMPT_VIEW_BRIDGE_H_

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#import "chrome/browser/ui/cocoa/passwords/account_chooser_view_controller.h"
#include "chrome/browser/ui/passwords/account_chooser_prompt.h"

@class AccountChooserViewController;

class PasswordPromptViewBridge : public AccountChooserPrompt,
                                 public ConstrainedWindowMacDelegate,
                                 public AccountChooserBridge {
 public:
  PasswordPromptViewBridge(PasswordDialogController* controller,
                           content::WebContents* web_contents);
  ~PasswordPromptViewBridge() override;

  // AccountChooserPrompt:
  void Show() override;
  void ControllerGone() override;

  // ConstrainedWindowMacDelegate:
  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override;

  // AccountChooserBridge:
  void PerformClose() override;
  PasswordDialogController* GetDialogController() override;
  net::URLRequestContextGetter* GetRequestContext() const override;

 private:
  PasswordDialogController* controller_;
  content::WebContents* web_contents_;

  scoped_ptr<ConstrainedWindowMac> constrained_window_;
  base::scoped_nsobject<AccountChooserViewController> view_controller_;
};

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_PASSWORD_PROMPT_VIEW_BRIDGE_H_
