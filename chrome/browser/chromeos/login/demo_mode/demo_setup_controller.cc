// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"

#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace {

constexpr const char kDemoDomain[] = "cros-demo-mode.com";
constexpr const char kDemoRequisition[] = "cros-demo-mode";

}  //  namespace

namespace chromeos {

DemoSetupController::DemoSetupController(Delegate* delegate)
    : delegate_(delegate) {}

DemoSetupController::~DemoSetupController() = default;

void DemoSetupController::EnrollOnline() {
  DCHECK(!enrollment_helper_);
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  connector->GetDeviceCloudPolicyManager()->SetDeviceRequisition(
      kDemoRequisition);
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_ATTESTATION;
  config.management_domain = kDemoDomain;

  enrollment_helper_ =
      EnterpriseEnrollmentHelper::Create(this, nullptr, config, kDemoDomain);
  enrollment_helper_->EnrollUsingAttestation();
}

void DemoSetupController::EnrollOffline() {
  DCHECK(!enrollment_helper_);
  // TODO(mukai): load the policy data from somewhere (maybe asynchronously)
  // and then set the loaded policy data into config. https://crbug.com/827290
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_OFFLINE_DEMO;
  config.management_domain = kDemoDomain;
  enrollment_helper_ = EnterpriseEnrollmentHelper::Create(
      this, nullptr /* ad_join_delegate */, config, kDemoDomain);
  enrollment_helper_->EnrollForOfflineDemo();
}

void DemoSetupController::OnAuthError(const GoogleServiceAuthError& error) {
  NOTREACHED();
}

void DemoSetupController::OnEnrollmentError(policy::EnrollmentStatus status) {
  LOG(ERROR) << "OnEnrollmentError: " << status.status() << ", "
             << status.client_status() << ", " << status.store_status() << ", "
             << status.validation_status() << ", " << status.lock_status();
  DCHECK(enrollment_helper_);
  enrollment_helper_.reset();
  if (delegate_)
    delegate_->OnSetupError();
}

void DemoSetupController::OnOtherError(
    EnterpriseEnrollmentHelper::OtherError error) {
  LOG(ERROR) << "Other error: " << error;
  DCHECK(enrollment_helper_);
  enrollment_helper_.reset();
  if (delegate_)
    delegate_->OnSetupError();
}

void DemoSetupController::OnDeviceEnrolled(
    const std::string& additional_token) {
  DCHECK(enrollment_helper_);
  enrollment_helper_.reset();
  if (delegate_)
    delegate_->OnSetupSuccess();
}

void DemoSetupController::OnMultipleLicensesAvailable(
    const EnrollmentLicenseMap& licenses) {
  NOTREACHED();
}

void DemoSetupController::OnDeviceAttributeUploadCompleted(bool success) {
  NOTREACHED();
}

void DemoSetupController::OnDeviceAttributeUpdatePermission(bool granted) {
  NOTREACHED();
}

}  //  namespace chromeos
