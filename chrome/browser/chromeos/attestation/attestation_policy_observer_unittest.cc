// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/chromeos/attestation/attestation_policy_observer.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/policy/cloud/mock_cloud_policy_client.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "content/public/test/test_browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

// A test key encoded as ASN.1 PrivateKeyInfo from PKCS #8.
const uint8 kTestKeyData[] = {
    0x30, 0x82, 0x01, 0x55, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x01, 0x3f, 0x30, 0x82, 0x01, 0x3b, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00,
    0xd9, 0xcd, 0xca, 0xcd, 0xc3, 0xea, 0xbe, 0x72, 0x79, 0x1c, 0x29, 0x37,
    0x39, 0x99, 0x1f, 0xd4, 0xb3, 0x0e, 0xf0, 0x7b, 0x78, 0x77, 0x0e, 0x05,
    0x3b, 0x65, 0x34, 0x12, 0x62, 0xaf, 0xa6, 0x8d, 0x33, 0xce, 0x78, 0xf8,
    0x47, 0x05, 0x1d, 0x98, 0xaa, 0x1b, 0x1f, 0x50, 0x05, 0x5b, 0x3c, 0x19,
    0x3f, 0x80, 0x83, 0x63, 0x63, 0x3a, 0xec, 0xcb, 0x2e, 0x90, 0x4f, 0xf5,
    0x26, 0x76, 0xf1, 0xd5, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x40, 0x64,
    0x29, 0xc2, 0xd9, 0x6b, 0xfe, 0xf9, 0x84, 0x75, 0x73, 0xe0, 0xf4, 0x77,
    0xb5, 0x96, 0xb0, 0xdf, 0x83, 0xc0, 0x4e, 0x57, 0xf1, 0x10, 0x6e, 0x91,
    0x89, 0x12, 0x30, 0x5e, 0x57, 0xff, 0x14, 0x59, 0x5f, 0x18, 0x86, 0x4e,
    0x4b, 0x17, 0x56, 0xfc, 0x8d, 0x40, 0xdd, 0x74, 0x65, 0xd3, 0xff, 0x67,
    0x64, 0xcb, 0x9c, 0xb4, 0x14, 0x8a, 0x06, 0xb7, 0x13, 0x45, 0x94, 0x16,
    0x7d, 0x3f, 0xe1, 0x02, 0x21, 0x00, 0xf6, 0x0f, 0x31, 0x6d, 0x06, 0xcc,
    0x3b, 0xa0, 0x44, 0x1f, 0xf5, 0xc2, 0x45, 0x2b, 0x10, 0x6c, 0xf9, 0x6f,
    0x8f, 0x87, 0x3d, 0xc0, 0x3b, 0x55, 0x13, 0x37, 0x80, 0xcd, 0x9f, 0xe1,
    0xb7, 0xd9, 0x02, 0x21, 0x00, 0xe2, 0x9a, 0x5f, 0xbf, 0x95, 0x74, 0xb5,
    0x7a, 0x6a, 0xa6, 0x97, 0xbd, 0x75, 0x8c, 0x97, 0x18, 0x24, 0xd6, 0x09,
    0xcd, 0xdc, 0xb5, 0x94, 0xbf, 0xe2, 0x78, 0xaa, 0x20, 0x47, 0x9f, 0x68,
    0x5d, 0x02, 0x21, 0x00, 0xaf, 0x8f, 0x97, 0x8c, 0x5a, 0xd5, 0x4d, 0x95,
    0xc4, 0x05, 0xa9, 0xab, 0xba, 0xfe, 0x46, 0xf1, 0xf9, 0xe7, 0x07, 0x59,
    0x4f, 0x4d, 0xe1, 0x07, 0x8a, 0x76, 0x87, 0x88, 0x2f, 0x13, 0x35, 0xc1,
    0x02, 0x20, 0x24, 0xc3, 0xd9, 0x2f, 0x13, 0x47, 0x99, 0x3e, 0x20, 0x59,
    0xa1, 0x1a, 0xeb, 0x1c, 0x81, 0x53, 0x38, 0x7e, 0xc5, 0x9e, 0x71, 0xe5,
    0xc0, 0x19, 0x95, 0xdb, 0xef, 0xf6, 0x46, 0xc8, 0x95, 0x3d, 0x02, 0x21,
    0x00, 0xaa, 0xb1, 0xff, 0x8a, 0xa2, 0xb2, 0x2b, 0xef, 0x9a, 0x83, 0x3f,
    0xc5, 0xbc, 0xd4, 0x6a, 0x07, 0xe8, 0xc7, 0x0b, 0x2e, 0xd4, 0x0f, 0xf8,
    0x98, 0x68, 0xe1, 0x04, 0xa8, 0x92, 0xd0, 0x10, 0xaa,
};

void DBusCallbackFalse(const BoolDBusMethodCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, false));
}

void DBusCallbackTrue(const BoolDBusMethodCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS, true));
}

void CertCallbackSuccess(const AttestationFlow::CertificateCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, true, "fake_cert"));
}

void StatusCallbackSuccess(
    const policy::CloudPolicyClient::StatusCallback& callback) {
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, true));
}

class FakeDBusData {
 public:
  explicit FakeDBusData(const std::string& data) : data_(data) {}

  void operator() (const CryptohomeClient::DataMethodCallback& callback) {
    MessageLoop::current()->PostTask(
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
  enum CertExpiryOptions {
    CERT_VALID,
    CERT_EXPIRING_SOON,
    CERT_EXPIRED
  };

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
      EXPECT_CALL(cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _))
          .WillRepeatedly(WithArgs<2>(Invoke(DBusCallbackTrue)));
      EXPECT_CALL(cryptohome_client_, TpmAttestationGetCertificate(_, _, _))
          .WillRepeatedly(WithArgs<2>(Invoke(FakeDBusData(certificate))));
    } else {
      EXPECT_CALL(cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _))
          .WillRepeatedly(WithArgs<2>(Invoke(DBusCallbackFalse)));
    }

    // Setup expected key payload queries.
    bool key_uploaded = (mock_options & MOCK_KEY_UPLOADED);
    std::string payload = CreatePayload();
    EXPECT_CALL(cryptohome_client_, TpmAttestationGetKeyPayload(_, _, _))
        .WillRepeatedly(WithArgs<2>(Invoke(
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
                  TpmAttestationSetKeyPayload(_, _, payload, _))
          .WillOnce(WithArgs<3>(Invoke(DBusCallbackTrue)));
    }

    // Setup expected key generations.  Again use WillOnce().  Key generation is
    // another costly operation and if it gets triggered more than once during
    // a single pass this indicates a logical problem in the observer.
    if (new_key) {
      EXPECT_CALL(attestation_flow_, GetCertificate(_, _, _))
          .WillOnce(WithArgs<2>(Invoke(CertCallbackSuccess)));
    }
  }

  void Run() {
    AttestationPolicyObserver observer(&policy_client_,
                                       &cryptohome_client_,
                                       &attestation_flow_);
    base::RunLoop().RunUntilIdle();
  }

  std::string CreatePayload() {
    AttestationKeyPayload proto;
    proto.set_is_certificate_uploaded(true);
    std::string serialized;
    proto.SerializeToString(&serialized);
    return serialized;
  }

  bool CreateCertificate(CertExpiryOptions options, std::string* certificate) {
    base::Time valid_start = base::Time::Now() - base::TimeDelta::FromDays(90);
    base::Time valid_expiry;
    switch (options) {
      case CERT_VALID:
        valid_expiry = base::Time::Now() + base::TimeDelta::FromDays(90);
        break;
      case CERT_EXPIRING_SOON:
        valid_expiry = base::Time::Now() + base::TimeDelta::FromDays(20);
        break;
      case CERT_EXPIRED:
        valid_expiry = base::Time::Now() - base::TimeDelta::FromDays(20);
        break;
      default:
        NOTREACHED();
    }
    scoped_ptr<crypto::RSAPrivateKey> test_key(
        crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(
            std::vector<uint8>(&kTestKeyData[0],
                               &kTestKeyData[arraysize(kTestKeyData)])));
    if (!test_key.get())
      return false;
    net::X509Certificate::OSCertHandle handle =
        net::x509_util::CreateSelfSignedCert(test_key->public_key(),
                                             test_key->key(),
                                             "CN=subject",
                                             12345,
                                             valid_start,
                                             valid_expiry);

    if (!handle)
      return false;
    bool result = net::X509Certificate::GetDEREncoded(handle, certificate);
    net::X509Certificate::FreeOSCertHandle(handle);
    return result;
  }

  MessageLoop message_loop_;
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
  ASSERT_TRUE(CreateCertificate(CERT_VALID, &certificate));
  SetupMocks(MOCK_KEY_EXISTS, certificate);
  Run();
}

TEST_F(AttestationPolicyObserverTest, KeyExistsAlreadyUploaded) {
  std::string certificate;
  ASSERT_TRUE(CreateCertificate(CERT_VALID, &certificate));
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED, certificate);
  Run();
}

TEST_F(AttestationPolicyObserverTest, KeyExistsCertExpiringSoon) {
  std::string certificate;
  ASSERT_TRUE(CreateCertificate(CERT_EXPIRING_SOON, &certificate));
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED | MOCK_NEW_KEY, certificate);
  Run();
}

TEST_F(AttestationPolicyObserverTest, KeyExistsCertExpired) {
  std::string certificate;
  ASSERT_TRUE(CreateCertificate(CERT_EXPIRED, &certificate));
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED | MOCK_NEW_KEY, certificate);
  Run();
}

TEST_F(AttestationPolicyObserverTest, IgnoreUnknownCertFormat) {
  SetupMocks(MOCK_KEY_EXISTS | MOCK_KEY_UPLOADED, "unsupported");
  Run();
}

}  // namespace attestation
}  // namespace chromeos
