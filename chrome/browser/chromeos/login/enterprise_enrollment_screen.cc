// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enterprise_enrollment_screen.h"

#include "chrome/browser/chromeos/login/screen_observer.h"

namespace chromeos {

EnterpriseEnrollmentScreen::EnterpriseEnrollmentScreen(
    WizardScreenDelegate* delegate)
    : DefaultViewScreen<EnterpriseEnrollmentView>(delegate) {}

EnterpriseEnrollmentScreen::~EnterpriseEnrollmentScreen() {
  if (view())
    view()->set_controller(NULL);
}

void EnterpriseEnrollmentScreen::CancelEnrollment() {
  ScreenObserver* observer = delegate()->GetObserver(this);
  observer->OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_CANCELLED);
}

void EnterpriseEnrollmentScreen::Show() {
  DefaultViewScreen<EnterpriseEnrollmentView>::Show();
  view()->set_controller(this);
}

}  // namespace chromeos
