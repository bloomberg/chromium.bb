// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_SCREEN_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"

namespace chromeos {

class ScreenObserver;

// The screen implementation that links the enterprise enrollment UI into the
// OOBE wizard.
class EnterpriseEnrollmentScreen
    : public WizardScreen,
      public EnterpriseEnrollmentScreenActor::Controller,
      public policy::CloudPolicySubsystem::Observer {
 public:
  EnterpriseEnrollmentScreen(ScreenObserver* observer,
                             EnterpriseEnrollmentScreenActor* actor);
  virtual ~EnterpriseEnrollmentScreen();

  void SetParameters(bool is_auto_enrollment,
                     const std::string& enrollment_user);

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // EnterpriseEnrollmentScreenActor::Controller implementation:
  virtual void OnOAuthTokenAvailable(const std::string& user,
                                     const std::string& token) OVERRIDE;
  virtual void OnConfirmationClosed(bool go_back_to_signin) OVERRIDE;
  virtual bool IsAutoEnrollment(std::string* user) OVERRIDE;

  // CloudPolicySubsystem::Observer implementation:
  virtual void OnPolicyStateChanged(
      policy::CloudPolicySubsystem::PolicySubsystemState state,
      policy::CloudPolicySubsystem::ErrorDetails error_details) OVERRIDE;

  // Used for testing.
  EnterpriseEnrollmentScreenActor* GetActor() {
    return actor_;
  }

 private:
  // Starts the Lockbox storage process.
  void WriteInstallAttributesData();

  // Kicks off the policy infrastructure to register with the service.
  void RegisterForDevicePolicy(const std::string& token);

  EnterpriseEnrollmentScreenActor* actor_;
  bool is_auto_enrollment_;
  bool is_showing_;
  std::string user_;
  int lockbox_init_duration_;
  scoped_ptr<policy::CloudPolicySubsystem::ObserverRegistrar> registrar_;
  base::WeakPtrFactory<EnterpriseEnrollmentScreen> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_SCREEN_H_
