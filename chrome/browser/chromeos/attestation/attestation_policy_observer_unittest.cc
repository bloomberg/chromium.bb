// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/chromeos/attestation/attestation_policy_observer.h"
#include "chrome/browser/chromeos/attestation/fake_certificate.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

const int64 kCertValid = 90;
const int64 kCertExpiringSoon = 20;
const int64 kCertExpired = -20;

void DBusCallbackFalse(const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void DBusCallbackTrue(const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void DBusCallbackError(const BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_FAILURE, false));
}

void CertCallbackSuccess(const AttestationFlow::CertificateCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, true, "fake_cert"));
}

void StatusCallbackSuccess(
    const policy::CloudPolicyClient::StatusCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, true));
}

class FakeDBusData {
 public:
  explicit FakeDBusData(const std::string& data) : data_(data) {}

  void operator() (const CryptohomeClient::DataMethodCallback& callback) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true, data_));
  }

 private:
  std::string data_;
};

}  // namespace

class AttestationPolicyObserverTest : public ::testing::Test {
 public:
  AttestationPolicyObserverTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
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
  enum MockOptions {
    MOCK_KEY_EXISTS = 1,          // Configure so a certified key exists.
    MOCK_KEY_UPLOADED = (1 << 1), // Configure so an upload has occurred.
    MOCK_NEW_KEY = (1 << 2)       // Configure expecting new key generation.
  };

  // Configures mock expectations according to |mock_options|.  If options
  // require that a certificate exists, |certificate| will be used.
  void SetupMocks(int mock_options, const std::string& certificate) {
    bool key_exists = (mock_options & MOCK_KEY_EXISTS);
    // Setup expected key / cert queries.
    if (key_exists) {
      EXPECT_CALL(cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _, _))
          .WillRepeatedly(WithArgs<3>(Invoke(DBusCallbackTrue)));
      EXPECT_CALL(cryptohome_client_, TpmAttestationGetCertificate(_, _, _, _))
          .WillRepeatedly(WithArgs<3>(Invoke(FakeDBusData(certificate))));
    } else {
      EXPECT_CALL(cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _, _))
          .WillRepeatedly(WithArgs<3>(Invoke(DBusCallbackFalse)));
    }

    // Setup expected key payload queries.
    bool key_uploaded = (mock_options & MOCK_KEY_UPLOADED);
    std::string payload = CreatePayload();
    EXPECT_CALL(cryptohome_client_, TpmAttestationGetKeyPayload(_, _, _, _))
        .WillRepeatedly(WithArgs<3>(Invoke(
            FakeDBusData(key_uploaded ? payload : ""))));

    // Setup expected key uploads.  Use WillOnce() so StrictMock will trigger an
    // error if our expectations are not met exactly.  We want to verify that
    // during a single run through the observer only one upload operation occurs
    // (because it is costly) and similarly, that the writing of the uploaded
    // status in the key payload matches the upload operation.
    bool new_key = (mock_options & MOCK_NEW_KEY);
    if (new_key || !key_uploaded) {
      EXPECT_CALL(policy_client_,
                  UploadCertificate(new_key ? "fake_cert" : certificate, _))
          .WillOnce(WithArgs<1>(Invoke(StatusCallbackSuccess)));
      EXPECT_CALL(cryptohome_client_,
                  TpmAttestationSetKeyPayload(_, _, _, payload, _))
          .WillOnce(WithArgs<4>(Invoke(DBusCallbackTrue)));
    }

    // Setup expected key generations.  Again use WillOnce().  Key generation is
    // another costly operation and if it gets triggered more than once during
    // a single pass this indicates a logical problem in the observer.
    if (new_key) {
      EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _, _, _))
          .WillOnce(WithArgs<4>(Invoke(CertCallbackSuccess)));
    }
  }

  void Run() {
    AttestationPolicyObserver observer(&policy_client_,
                                       &cryptohome_client_,
                                       &attestation_flow_);
    observer.set_retry_delay(0);
    base::RunLoop().RunUntilIdle();
  }

  std::string CreatePayload() {
    AttestationKeyPayload proto;
    proto.set_is_certificate_uploaded(true);
    std::string serialized;
    proto.SerializeToString(&serialized);
    return serialized;
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  ScopedTestDeviceSettingsService test_device_settings_service_;
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
  SetupMocks(MOCK_NEW_KEY, "");
  Run();
}

TEST_F(AttestationPolicyObserverTest, KeyExistsNotUploaded) {
  std::string certificate;
  ASSERT_TRUE(GetFakeCertificate(base::TimeDelta::FromDays(kCertValid),
                                 &certificate));
  SetupMocks(MOCK_KEY_EXISTS, certificate);
  Run();
}

TEST_F(AttestationPolicyObserverTest, KeyExistsAlreadyUploaded) {
  std::string certificate;
  ASSERT_TRUE(GetFakeCertificate(base::TimeDelta::FromDays(kCertValid),
                                 &certificate));
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED, certificate);
  Run();
}

TEST_F(AttestationPolicyObserverTest, KeyExistsCertExpiringSoon) {
  std::string certificate;
  ASSERT_TRUE(GetFakeCertificate(base::TimeDelta::FromDays(kCertExpiringSoon),
                                 &certificate));
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED | MOCK_NEW_KEY, certificate);
  Run();
}

TEST_F(AttestationPolicyObserverTest, KeyExistsCertExpired) {
  std::string certificate;
  ASSERT_TRUE(GetFakeCertificate(base::TimeDelta::FromDays(kCertExpired),
                                 &certificate));
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED | MOCK_NEW_KEY, certificate);
  Run();
}

TEST_F(AttestationPolicyObserverTest, IgnoreUnknownCertFormat) {
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED, "unsupported");
  Run();
}

TEST_F(AttestationPolicyObserverTest, DBusFailureRetry) {
  SetupMocks(MOCK_NEW_KEY, "");
  // Simulate a DBus failure.
  EXPECT_CALL(cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _, _))
      .WillOnce(WithArgs<3>(Invoke(DBusCallbackError)))
      .WillRepeatedly(WithArgs<3>(Invoke(DBusCallbackFalse)));
  Run();
}

}  // namespace attestation
}  // namespace chromeos
