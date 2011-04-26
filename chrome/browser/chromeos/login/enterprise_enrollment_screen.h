// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_SCREEN_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/enterprise_enrollment_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"

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

  // Returns whether the GAIA login should be prepolutated with an user and if
  // yes which one.
  virtual bool GetInitialUser(std::string* user) = 0;
};

// The screen implementation that links the enterprise enrollment UI into the
// OOBE wizard.
class EnterpriseEnrollmentScreen
    : public ViewScreen<EnterpriseEnrollmentView>,
      public EnterpriseEnrollmentController,
      public GaiaAuthConsumer,
      public policy::CloudPolicySubsystem::Observer {
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
  virtual bool GetInitialUser(std::string* user) OVERRIDE;

  // GaiaAuthConsumer implementation:
  virtual void OnClientLoginSuccess(const ClientLoginResult& result) OVERRIDE;
  virtual void OnClientLoginFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  virtual void OnIssueAuthTokenSuccess(const std::string& service,
                                       const std::string& auth_token) OVERRIDE;
  virtual void OnIssueAuthTokenFailure(
      const std::string& service,
      const GoogleServiceAuthError& error) OVERRIDE;

  // CloudPolicySubsystem::Observer implementation:
  virtual void OnPolicyStateChanged(
      policy::CloudPolicySubsystem::PolicySubsystemState state,
      policy::CloudPolicySubsystem::ErrorDetails error_details) OVERRIDE;

 protected:
  // Overriden from ViewScreen:
  virtual EnterpriseEnrollmentView* AllocateView() OVERRIDE;

 private:
  void HandleAuthError(const GoogleServiceAuthError& error);

  // Starts the Lockbox storage process.
  void WriteInstallAttributesData();

  scoped_ptr<GaiaAuthFetcher> auth_fetcher_;
  std::string user_;
  std::string captcha_token_;
  scoped_ptr<policy::CloudPolicySubsystem::ObserverRegistrar> registrar_;
  ScopedRunnableMethodFactory<EnterpriseEnrollmentScreen>
      runnable_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_SCREEN_H_
