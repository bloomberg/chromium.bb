// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

WizardInProcessBrowserTest::WizardInProcessBrowserTest(const char* screen_name)
    : screen_name_(screen_name),
      host_(NULL) {
}

void WizardInProcessBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  command_line->AppendSwitch(switches::kNoStartupWindow);
}

void WizardInProcessBrowserTest::SetUpOnMainThread() {
  SetUpWizard();

  WizardController::SetZeroDelays();

  if (!screen_name_.empty()) {
    ShowLoginWizard(screen_name_, gfx::Size(1024, 600));
    host_ = BaseLoginDisplayHost::default_host();
  }
}

void WizardInProcessBrowserTest::CleanUpOnMainThread() {
  // LoginDisplayHost owns controllers and all windows.
  MessageLoopForUI::current()->DeleteSoon(FROM_HERE, host_);

  ui_test_utils::RunMessageLoop();
}

}  // namespace chromeos
