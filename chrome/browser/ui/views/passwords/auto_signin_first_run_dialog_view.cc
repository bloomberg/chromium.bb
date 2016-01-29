// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/auto_signin_first_run_dialog_view.h"

AutoSigninFirstRunDialogView::AutoSigninFirstRunDialogView() {
}

AutoSigninFirstRunDialogView::~AutoSigninFirstRunDialogView() {
}

void AutoSigninFirstRunDialogView::ShowAutoSigninPrompt() {
  // TODO(vasilii): implement.
}

void AutoSigninFirstRunDialogView::ControllerGone() {
  // TODO(vasilii): implement.
}

AutoSigninFirstRunPrompt* CreateAutoSigninPromptView(
    PasswordDialogController* controller, content::WebContents* web_contents) {
  return new AutoSigninFirstRunDialogView;
}
