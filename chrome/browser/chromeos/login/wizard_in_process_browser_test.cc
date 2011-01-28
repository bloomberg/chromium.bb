// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"

#include "base/message_loop.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/test/ui_test_utils.h"

namespace chromeos {

WizardInProcessBrowserTest::WizardInProcessBrowserTest(const char* screen_name)
    : screen_name_(screen_name),
      controller_(NULL) {
}

Browser* WizardInProcessBrowserTest::CreateBrowser(Profile* profile) {
  SetUpWizard();

  WizardController::SetZeroDelays();

  if (!screen_name_.empty()) {
    browser::ShowLoginWizard(screen_name_.c_str(), gfx::Size(1024, 600));
    controller_ = WizardController::default_controller();
  }
  return NULL;
}

void WizardInProcessBrowserTest::CleanUpOnMainThread() {
  delete controller_;

  // Observers and what not are notified after the views are deleted, which
  // happens after a delay (because they are contained in a WidgetGtk which
  // delays deleting itself). Run the message loop until we know the wizard
  // has been deleted.
  ui_test_utils::WaitForNotification(
      NotificationType::WIZARD_CONTENT_VIEW_DESTROYED);
}

}  // namespace chromeos
