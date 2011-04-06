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
  // Runs authentication with the given parameters.
  virtual void Authenticate(const std::string& user,
                            const std::string& password,
                            const std::string& captcha,
                            const std::string& access_code) = 0;

  // Cancels the enrollment operation.
  virtual void CancelEnrollment() = 0;

  // Closes the confirmation window.
  virtual void CloseConfirmation() = 0;
};

// The screen implementation that links the enterprise enrollment UI into the
// OOBE wizard.
class EnterpriseEnrollmentScreen
    : public ViewScreen<EnterpriseEnrollmentView>,
      public EnterpriseEnrollmentController {
 public:
  explicit EnterpriseEnrollmentScreen(WizardScreenDelegate* delegate);
  virtual ~EnterpriseEnrollmentScreen();

  // EnterpriseEnrollmentController implementation:
  virtual void Authenticate(const std::string& user,
                            const std::string& password,
                            const std::string& captcha,
                            const std::string& access_code) OVERRIDE;
  virtual void CancelEnrollment() OVERRIDE;
  virtual void CloseConfirmation() OVERRIDE;

 protected:
  // Overriden from ViewScreen:
  virtual EnterpriseEnrollmentView* AllocateView() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_SCREEN_H_
