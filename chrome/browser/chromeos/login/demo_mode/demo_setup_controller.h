// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"

namespace chromeos {

// Controlls enrollment flow for setting up Demo Mode.
class DemoSetupController
    : public EnterpriseEnrollmentHelper::EnrollmentStatusConsumer {
 public:
  // Delegate that will be notified about result of setup flow when it is
  // finished.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the setup flow finished with error.
    virtual void OnSetupError() = 0;

    // Called when the setup flow finished successfully.
    virtual void OnSetupSuccess() = 0;
  };

  explicit DemoSetupController(Delegate* delegate);
  ~DemoSetupController() override;

  // Initiates online enrollment that registers and sets up the device in the
  // Demo Mode domain.
  void EnrollOnline();

  // Initiates offline enrollment that locks the device and sets up offline
  // policies required by Demo Mode. It requires no network connectivity since
  // and all setup will be done locally.
  void EnrollOffline();

  // EnterpriseEnrollmentHelper::EnrollmentStatusConsumer:
  void OnDeviceEnrolled(const std::string& additional_token) override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnOtherError(EnterpriseEnrollmentHelper::OtherError error) override;
  void OnDeviceAttributeUploadCompleted(bool success) override;
  void OnDeviceAttributeUpdatePermission(bool granted) override;
  void OnMultipleLicensesAvailable(
      const EnrollmentLicenseMap& licenses) override;

 private:
  Delegate* delegate_ = nullptr;

  std::unique_ptr<EnterpriseEnrollmentHelper> enrollment_helper_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupController);
};

}  //  namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
