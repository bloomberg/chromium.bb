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
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/attestation/enrollment_policy_observer.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

void StatusCallbackSuccess(
    const policy::CloudPolicyClient::StatusCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(callback, true));
}

}  // namespace

class EnrollmentPolicyObserverTest : public DeviceSettingsTestBase {
 public:
  EnrollmentPolicyObserverTest() {
    policy_client_.SetDMToken("fake_dm_token");

    std::vector<uint8_t> eid;
    EXPECT_TRUE(base::HexStringToBytes(kEnrollmentId, &eid));
    enrollment_id_.assign(reinterpret_cast<const char*>(eid.data()),
                          eid.size());
    cryptohome_client_.set_tpm_attestation_enrollment_id(
        true /* ignore_cache */, enrollment_id_);
  }

 protected:
  static constexpr char kEnrollmentId[] =
      "6fcc0ebddec3db9500cf82476d594f4d60db934c5b47fa6085c707b2a93e205b";

  void SetUpEnrollmentIdNeeded(bool enrollment_id_needed) {
    if (enrollment_id_needed) {
      EXPECT_CALL(policy_client_,
                  UploadEnterpriseEnrollmentId(enrollment_id_, _))
          .WillOnce(WithArgs<1>(Invoke(StatusCallbackSuccess)));
    }
    SetUpDevicePolicy(enrollment_id_needed);
  }

  void SetUpDevicePolicy(bool enrollment_id_needed) {
    device_policy_.policy_data().set_enrollment_id_needed(enrollment_id_needed);
    ReloadDevicePolicy();
    ReloadDeviceSettings();
  }

  void Run() {
    EnrollmentPolicyObserver observer(
        &policy_client_, &device_settings_service_, &cryptohome_client_);
    observer.set_retry_limit(3);
    observer.set_retry_delay(0);
    base::RunLoop().RunUntilIdle();
  }

  FakeCryptohomeClient cryptohome_client_;
  StrictMock<policy::MockCloudPolicyClient> policy_client_;
  std::string enrollment_id_;
};

constexpr char EnrollmentPolicyObserverTest::kEnrollmentId[];

TEST_F(EnrollmentPolicyObserverTest, UploadEnterpriseEnrollmentId) {
  SetUpEnrollmentIdNeeded(true);
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, FeatureDisabled) {
  SetUpEnrollmentIdNeeded(false);
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, UnregisteredPolicyClient) {
  policy_client_.SetDMToken("");
  Run();
}

TEST_F(EnrollmentPolicyObserverTest, DBusFailureRetry) {
  SetUpEnrollmentIdNeeded(true);

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
