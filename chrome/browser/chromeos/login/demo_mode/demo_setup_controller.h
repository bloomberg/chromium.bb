// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace chromeos {

// Controlls enrollment flow for setting up Demo Mode.
class DemoSetupController
    : public EnterpriseEnrollmentHelper::EnrollmentStatusConsumer,
      public policy::CloudPolicyStore::Observer {
 public:
  // Type of demo mode enrollment.
  enum class EnrollmentType {
    // Online enrollment into demo mode that is established with DMServer.
    kOnline,
    // Offline enrollment into demo mode that is established locally.
    kOffline,
  };

  // Type of demo mode setup error.
  enum class DemoSetupError {
    // Recoverable or temporary demo mode setup error. Another attempt to setup
    // demo mode may succeed.
    kRecoverable,
    // Fatal demo mode setup error. Device requires powerwash to recover from
    // the resulting state.
    kFatal,
  };

  // Demo mode setup callbacks.
  using OnSetupSuccess = base::OnceClosure;
  using OnSetupError = base::OnceCallback<void(DemoSetupError)>;

  // Domain that demo mode devices are enrolled into.
  static constexpr char kDemoModeDomain[] = "cros-demo-mode.com";

  // Utility method that returns whether demo mode setup flow is in progress in
  // OOBE.
  static bool IsOobeDemoSetupFlowInProgress();

  DemoSetupController();
  ~DemoSetupController() override;

  // Sets enrollment type that will be used to setup the device. It has to be
  // set before calling Enroll().
  void set_enrollment_type(EnrollmentType enrollment_type) {
    enrollment_type_ = enrollment_type;
  }

  // Whether offline enrollment is used for setup.
  bool IsOfflineEnrollment() const;

  // Initiates enrollment that sets up the device in the demo mode domain. The
  // |enrollment_type_| determines whether online or offline setup will be
  // performed and it should be set with set_enrollment_type() before calling
  // Enroll(). |on_setup_success| will be called when enrollment finishes
  // successfully. |on_setup_error| will be called when enrollment finishes with
  // an error.
  void Enroll(OnSetupSuccess on_setup_success, OnSetupError on_setup_error);

  // EnterpriseEnrollmentHelper::EnrollmentStatusConsumer:
  void OnDeviceEnrolled(const std::string& additional_token) override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnOtherError(EnterpriseEnrollmentHelper::OtherError error) override;
  void OnDeviceAttributeUploadCompleted(bool success) override;
  void OnDeviceAttributeUpdatePermission(bool granted) override;
  void OnMultipleLicensesAvailable(
      const EnrollmentLicenseMap& licenses) override;

  void SetDeviceLocalAccountPolicyStoreForTest(policy::CloudPolicyStore* store);
  void SetOfflineDataDirForTest(const base::FilePath& offline_dir);

 private:
  // Initiates online enrollment that registers and sets up the device in the
  // demo mode domain.
  void EnrollOnline();

  // Initiates offline enrollment that locks the device and sets up offline
  // policies required by demo mode. It requires no network connectivity since
  // all setup will be done locally. The policy files will be loaded from the
  // |policy_dir|.
  void EnrollOffline(const base::FilePath& policy_dir);

  // Called when the checks of policy files for the offline demo mode is done.
  void OnOfflinePolicyFilesExisted(std::string* message, bool ok);

  // Called when the device local account policy for the offline demo mode is
  // loaded.
  void OnDeviceLocalAccountPolicyLoaded(base::Optional<std::string> blob);

  // Called when device is marked as registered and the second part of OOBE flow
  // is completed. This is the last step of demo mode setup flow.
  void OnDeviceRegistered();

  // Finish the flow with an error message.
  void SetupFailed(const std::string& message, DemoSetupError error);

  // Clears the internal state.
  void Reset();

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  // Enrollment type that will be performed when Enroll() is called. Should be
  // set explicitly.
  base::Optional<EnrollmentType> enrollment_type_;

  // Callback to call when enrollment finishes with an error.
  OnSetupError on_setup_error_;

  // Callback to call when enrollment finishes successfully.
  OnSetupSuccess on_setup_success_;

  // The mode of the current enrollment flow.
  policy::EnrollmentConfig::Mode mode_ = policy::EnrollmentConfig::MODE_NONE;

  // The directory which contains the policy blob files for the offline
  // enrollment (i.e. device_policy and local_account_policy). Should be empty
  // on the online enrollment.
  base::FilePath policy_dir_;

  // The directory containing policy blob files used for testing.
  base::FilePath policy_dir_for_tests_;

  // The CloudPolicyStore for the device local account for the offline policy.
  policy::CloudPolicyStore* device_local_account_policy_store_ = nullptr;

  std::unique_ptr<EnterpriseEnrollmentHelper> enrollment_helper_;

  base::WeakPtrFactory<DemoSetupController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupController);
};

}  //  namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
