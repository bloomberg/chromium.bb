// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/attestation/attestation_policy_observer.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/policy/cloud/mock_cloud_policy_client.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

void DBusCallbackFalse(const BoolDBusMethodCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void DBusCallbackTrue(const BoolDBusMethodCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void DBusDataCallback(const CryptohomeClient::DataMethodCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true, "fake"));
}

void CertCallbackSuccess(const AttestationFlow::CertificateCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, true, "fake_cert"));
}

}  // namespace

class AttestationPolicyObserverTest : public ::testing::Test {
 public:
  AttestationPolicyObserverTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_) {
    // Remove the real DeviceSettingsProvider and replace it with a stub.
    CrosSettings* cros_settings = CrosSettings::Get();
    device_settings_provider_ =
        cros_settings->GetProvider(kDeviceAttestationEnabled);
    cros_settings->RemoveSettingsProvider(device_settings_provider_);
    cros_settings->AddSettingsProvider(&stub_settings_provider_);
    cros_settings->SetBoolean(kDeviceAttestationEnabled, true);
    policy_client_.SetDMToken("fake_dm_token");
  }

  virtual ~AttestationPolicyObserverTest() {
    // Restore the real DeviceSettingsProvider.
    CrosSettings* cros_settings = CrosSettings::Get();
    cros_settings->RemoveSettingsProvider(&stub_settings_provider_);
    cros_settings->AddSettingsProvider(device_settings_provider_);
  }

 protected:
  void Run() {
    AttestationPolicyObserver observer(&policy_client_,
                                       &cryptohome_client_,
                                       &attestation_flow_);
    base::RunLoop().RunUntilIdle();
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  ScopedTestCrosSettings test_cros_settings_;
  CrosSettingsProvider* device_settings_provider_;
  StubCrosSettingsProvider stub_settings_provider_;
  StrictMock<MockCryptohomeClient> cryptohome_client_;
  StrictMock<MockAttestationFlow> attestation_flow_;
  StrictMock<policy::MockCloudPolicyClient> policy_client_;
};

TEST_F(AttestationPolicyObserverTest, FeatureDisabled) {
  CrosSettings* cros_settings = CrosSettings::Get();
  cros_settings->SetBoolean(kDeviceAttestationEnabled, false);
  Run();
}

TEST_F(AttestationPolicyObserverTest, UnregisteredPolicyClient) {
  policy_client_.SetDMToken("");
  Run();
}

TEST_F(AttestationPolicyObserverTest, NewCertificate) {
  EXPECT_CALL(cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _))
      .WillOnce(WithArgs<2>(Invoke(DBusCallbackFalse)));
  EXPECT_CALL(attestation_flow_, GetCertificate(_, _))
      .WillOnce(WithArgs<1>(Invoke(CertCallbackSuccess)));
  Run();
}

TEST_F(AttestationPolicyObserverTest, KeyExists) {
  EXPECT_CALL(cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _))
      .WillOnce(WithArgs<2>(Invoke(DBusCallbackTrue)));
  EXPECT_CALL(cryptohome_client_, TpmAttestationGetCertificate(_, _, _))
      .WillOnce(WithArgs<2>(Invoke(DBusDataCallback)));
  Run();
}

}  // namespace attestation
}  // namespace chromeos
