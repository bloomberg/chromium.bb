// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_DIALOGS_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_TAB_DIALOGS_COCOA_H_

#include "chrome/browser/ui/tab_dialogs.h"

// Cocoa implementation of TabDialogs interface.
class TabDialogsCocoa : public TabDialogs {
 public:
  explicit TabDialogsCocoa(content::WebContents* contents);
  ~TabDialogsCocoa() override;

  // TabDialogs:
  void ShowCollectedCookies() override;
  void ShowHungRendererDialog() override;
  void HideHungRendererDialog() override;
  void ShowProfileSigninConfirmation(
      Browser* browser,
      Profile* profile,
      const std::string& username,
      ui::ProfileSigninConfirmationDelegate* delegate) override;
  void ShowManagePasswordsBubble(bool user_action) override;
  void HideManagePasswordsBubble() override;

 private:
  content::WebContents* web_contents_;  // Weak. Owns this.

  DISALLOW_COPY_AND_ASSIGN(TabDialogsCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_TAB_DIALOGS_COCOA_H_
