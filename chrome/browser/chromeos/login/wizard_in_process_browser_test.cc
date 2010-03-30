// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"

#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/views/browser_dialogs.h"

namespace chromeos {

WizardInProcessBrowserTest::WizardInProcessBrowserTest(const char* screen_name)
    : screen_name_(screen_name),
      controller_(NULL) {
}

Browser* WizardInProcessBrowserTest::CreateBrowser(Profile* profile) {
  SetUpWizard();

  browser::ShowLoginWizard(screen_name_.c_str(), gfx::Size(1024, 600));
  controller_ = WizardController::default_controller();
  return NULL;
}

void WizardInProcessBrowserTest::CleanUpOnMainThread() {
  delete controller_;
}

}  // namespace chromeos
