// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_SIGNIN_CONFIRMATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_SIGNIN_CONFIRMATION_UI_H_

#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"

// A WebUI tab-modal dialog to confirm signin with a managed account.
class ProfileSigninConfirmationUI : public ConstrainedWebDialogUI {
 public:
  explicit ProfileSigninConfirmationUI(content::WebUI* web_ui);
  virtual ~ProfileSigninConfirmationUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileSigninConfirmationUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_SIGNIN_CONFIRMATION_UI_H_
