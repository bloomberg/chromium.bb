// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/attestation/enrollment_policy_observer.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

void CertCallbackSuccess(const AttestationFlow::CertificateCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, true, "fake_cert"));
}

void StatusCallbackSuccess(
    const policy::CloudPolicyClient::StatusCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(callback, true));
}

}  // namespace

class EnrollmentPolicyObserverTest : public ::testing::Test {
 public:
  EnrollmentPolicyObserverTest() {
    settings_helper_.ReplaceProvider(kDeviceEnrollmentIdNeeded);
    settings_helper_.SetBoolean(kDeviceEnrollmentIdNeeded, true);
    policy_client_.SetDMToken("fake_dm_token");
  }

 protected:
  // Configures mock expectations when identification for enrollment is needed.
  void SetupMocks() {
    EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _))
        .WillOnce(WithArgs<4>(Invoke(CertCallbackSuccess)));
    EXPECT_CALL(policy_client_,
                UploadEnterpriseEnrollmentCertificate("fake_cert", _))
        .WillOnce(WithArgs<1>(Invoke(StatusCallbackSuccess)));
  }

  void Run() {
    EnrollmentPolicyObserver observer(&policy_client_, &cryptohome_client_,
                                      &attestation_flow_);
    observer.set_retry_delay(0);
    base::RunLoop().RunUntilIdle();
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  ScopedCrosSettingsTestHelper settings_helper_;
  FakeCryptohomeClient cryptohome_client_;
  StrictMock<MockAttestationFlow> attestation_flow_;
  StrictMock<policy::MockCloudPolicyClient> policy_client_;
};

TEST_F(EnrollmentPolicyObserverTest, FeatureDisabled) {
  settings_helper_.SetBoolean(kDeviceEnrollmentIdNeeded, false);
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, UnregisteredPolicyClient) {
  policy_client_.SetDMToken("");
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, DBusFailureRetry) {
  SetupMocks();
  // Simulate a DBus failure.
  cryptohome_client_.SetServiceIsAvailable(false);

  // Emulate delayed service initialization.
  // Run() instantiates an Observer, which synchronously calls
  // TpmAttestationDoesKeyExist() and fails. During this call, we make the
  // service available in the next run, so on retry, it will successfully
  // return the result.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](FakeCryptohomeClient* cryptohome_client) {
                       cryptohome_client->SetServiceIsAvailable(true);
                     },
                     base::Unretained(&cryptohome_client_)));
  Run();
}

}  // namespace attestation
}  // namespace chromeos
