// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_SCREEN_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/enterprise_enrollment_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"

namespace chromeos {

// Controller interface for driving the enterprise enrollment UI.
class EnterpriseEnrollmentController {
 public:
  // Cancels the enrollment operation.
  virtual void CancelEnrollment() = 0;
};

// The screen implementation that links the enterprise enrollment UI into the
// OOBE wizard.
class EnterpriseEnrollmentScreen
    : public DefaultViewScreen<EnterpriseEnrollmentView>,
      public EnterpriseEnrollmentController {
 public:
  explicit EnterpriseEnrollmentScreen(WizardScreenDelegate* delegate);
  virtual ~EnterpriseEnrollmentScreen();

  // Overriden from ViewScreen:
  void Show() OVERRIDE;

  // EnterpriseEnrollmentController implementation:
  void CancelEnrollment() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_SCREEN_H_
