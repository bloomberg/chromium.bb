// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/user_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "extensions/browser/notification_types.h"
#include "net/base/net_errors.h"
#include "net/cert/nss_cert_database.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

// The test extension has a certificate referencing this private key which will
// be stored in the user's token in the test setup.
//
// openssl genrsa > privkey.pem
// openssl pkcs8 -inform pem -in privkey.pem -topk8
//   -outform der -out privkey8.der -nocrypt
// xxd -i privkey8.der
const unsigned char privateKeyPkcs8User[] = {
    0x30, 0x82, 0x01, 0x55, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x01, 0x3f, 0x30, 0x82, 0x01, 0x3b, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00,
    0xc7, 0xc1, 0x4d, 0xd5, 0xdc, 0x3a, 0x2e, 0x1f, 0x42, 0x30, 0x3d, 0x21,
    0x1e, 0xa2, 0x1f, 0x60, 0xcb, 0x71, 0x11, 0x53, 0xb0, 0x75, 0xa0, 0x62,
    0xfe, 0x5e, 0x0a, 0xde, 0xb0, 0x0f, 0x48, 0x97, 0x5e, 0x42, 0xa7, 0x3a,
    0xd1, 0xca, 0x4c, 0xe3, 0xdb, 0x5f, 0x31, 0xc2, 0x99, 0x08, 0x89, 0xcd,
    0x6d, 0x20, 0xaa, 0x75, 0xe6, 0x2b, 0x98, 0xd2, 0xf3, 0x7b, 0x4b, 0xe5,
    0x9b, 0xfe, 0xe2, 0x6d, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x40, 0x4a,
    0xf5, 0x76, 0x10, 0xe7, 0xb8, 0x89, 0x70, 0x3f, 0x75, 0x3c, 0xab, 0x3e,
    0x04, 0x96, 0x83, 0xcb, 0x34, 0x1d, 0xcd, 0x6a, 0xed, 0x69, 0x07, 0x5c,
    0xee, 0xcb, 0x63, 0x6f, 0x6b, 0xfc, 0xcf, 0xee, 0xa2, 0xc4, 0x67, 0x05,
    0x68, 0x4d, 0x21, 0x7e, 0x3e, 0xde, 0x74, 0x72, 0xf8, 0x04, 0x35, 0x66,
    0x1e, 0x6b, 0x1d, 0xef, 0x77, 0xf7, 0x33, 0xf0, 0x35, 0xcf, 0x35, 0x6e,
    0x53, 0x3f, 0x9d, 0x02, 0x21, 0x00, 0xee, 0x48, 0x67, 0x1b, 0x24, 0x6e,
    0x3d, 0x7b, 0xa0, 0xc3, 0xee, 0x8a, 0x2e, 0xc7, 0xd0, 0xa1, 0xdb, 0x25,
    0x31, 0x12, 0x99, 0x43, 0x06, 0x3c, 0xb0, 0x80, 0x35, 0x2b, 0xf4, 0xc5,
    0xa2, 0xd3, 0x02, 0x21, 0x00, 0xd6, 0x9b, 0x8b, 0x75, 0x91, 0x52, 0xd4,
    0xf0, 0x76, 0xcf, 0xa2, 0xbe, 0xa6, 0xaf, 0x72, 0x6c, 0x52, 0xf9, 0xc9,
    0x0e, 0xea, 0x4a, 0x4c, 0xd2, 0xdf, 0x25, 0x70, 0xc6, 0x66, 0x35, 0x9d,
    0xbf, 0x02, 0x21, 0x00, 0xe8, 0x9e, 0x40, 0x21, 0xcc, 0x37, 0xde, 0xc7,
    0xd1, 0x13, 0x55, 0xcd, 0x0a, 0x8c, 0x40, 0xcd, 0xb1, 0xed, 0xa5, 0xf1,
    0x7d, 0x33, 0x64, 0x64, 0x5c, 0xfe, 0x5c, 0x6a, 0x34, 0x03, 0xb8, 0xc7,
    0x02, 0x20, 0x17, 0xe1, 0xb5, 0x52, 0x3e, 0xfa, 0xc5, 0xc1, 0x80, 0xa7,
    0x38, 0x88, 0x18, 0xca, 0x7b, 0x64, 0x3c, 0x93, 0x99, 0x61, 0x34, 0x87,
    0x52, 0x27, 0x41, 0x37, 0xcc, 0x65, 0xf7, 0xa7, 0xcd, 0xc7, 0x02, 0x21,
    0x00, 0x8a, 0x17, 0x7f, 0xf9, 0x45, 0xf3, 0xfd, 0xf7, 0x96, 0x62, 0xf3,
    0x7a, 0x09, 0xfb, 0xe9, 0x9e, 0xc7, 0x7a, 0x1f, 0x53, 0x1a, 0xb8, 0xd5,
    0x88, 0x9d, 0xd4, 0x79, 0x57, 0x88, 0x68, 0x72, 0x6f};

// The test extension has a certificate referencing this private key which will
// be stored in the system token in the test setup.
const unsigned char privateKeyPkcs8System[] = {
    0x30, 0x82, 0x01, 0x54, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x01, 0x3e, 0x30, 0x82, 0x01, 0x3a, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00,
    0xe8, 0xb3, 0x04, 0xb1, 0xad, 0xef, 0x6b, 0xe5, 0xbe, 0xc9, 0x05, 0x75,
    0x07, 0x41, 0xf5, 0x70, 0x50, 0xc2, 0xe8, 0xee, 0xeb, 0x09, 0x9d, 0x49,
    0x64, 0x4c, 0x60, 0x61, 0x80, 0xbe, 0xc5, 0x41, 0xf3, 0x8c, 0x57, 0x90,
    0x3a, 0x44, 0x62, 0x6d, 0x51, 0xb8, 0xbb, 0xc6, 0x9a, 0x16, 0xdf, 0xf9,
    0xce, 0xe3, 0xb8, 0x8c, 0x2e, 0xa2, 0x16, 0xc8, 0xed, 0xc7, 0xf8, 0x4f,
    0xbd, 0xd3, 0x6e, 0x63, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x40, 0x76,
    0xc9, 0x83, 0xf8, 0xeb, 0xd0, 0x8f, 0xa4, 0xdd, 0x4a, 0xa2, 0xe5, 0x85,
    0xc9, 0xee, 0xef, 0xe1, 0xda, 0x4d, 0xac, 0x41, 0x01, 0x4c, 0x70, 0x7d,
    0xa9, 0xdb, 0x7d, 0x8a, 0x8a, 0x58, 0x09, 0x04, 0x45, 0x43, 0xa4, 0xf3,
    0xb4, 0x98, 0xf6, 0x34, 0x68, 0x5f, 0xc1, 0xc2, 0xa7, 0x86, 0x3e, 0xec,
    0x84, 0x0b, 0x18, 0xbc, 0xb1, 0xee, 0x6f, 0x3f, 0xb1, 0x6d, 0xbc, 0x3e,
    0xbf, 0x6d, 0x31, 0x02, 0x21, 0x00, 0xff, 0x9d, 0x90, 0x4f, 0x0e, 0xe8,
    0x7e, 0xf3, 0x38, 0xa7, 0xec, 0x73, 0x80, 0xf9, 0x39, 0x2c, 0xaa, 0x33,
    0x91, 0x72, 0x10, 0x7c, 0x8b, 0xc3, 0x61, 0x6d, 0x40, 0x96, 0xac, 0xb3,
    0x5e, 0xc9, 0x02, 0x21, 0x00, 0xe9, 0x0c, 0xa1, 0x34, 0xf2, 0x43, 0x3c,
    0x74, 0xec, 0x1a, 0xf6, 0x80, 0x8e, 0x50, 0x10, 0x6d, 0x55, 0x64, 0xce,
    0x47, 0x4a, 0x1e, 0x34, 0x27, 0x6c, 0x49, 0x79, 0x6a, 0x23, 0xc6, 0x9d,
    0xcb, 0x02, 0x20, 0x48, 0xda, 0xa8, 0xc1, 0xcf, 0xb6, 0xf6, 0x4f, 0xee,
    0x4a, 0xf6, 0x3a, 0xa9, 0x7c, 0xdf, 0x0d, 0xda, 0xe8, 0xdd, 0xc0, 0x8b,
    0xf0, 0x63, 0x89, 0x69, 0x60, 0x51, 0x33, 0x60, 0xbf, 0xb2, 0xf9, 0x02,
    0x21, 0x00, 0xb4, 0x77, 0x81, 0x46, 0x7c, 0xec, 0x30, 0x1e, 0xe2, 0xcf,
    0x26, 0x5f, 0xfa, 0xd4, 0x69, 0x44, 0x21, 0x42, 0x84, 0xb2, 0x93, 0xe4,
    0xbb, 0xc2, 0x63, 0x8a, 0xaa, 0x28, 0xd5, 0x37, 0x72, 0xed, 0x02, 0x20,
    0x16, 0xde, 0x3d, 0x57, 0xc5, 0xd5, 0x3d, 0x90, 0x8b, 0xfd, 0x90, 0x3b,
    0xd8, 0x71, 0x69, 0x5e, 0x8d, 0xb4, 0x48, 0x1c, 0xa4, 0x01, 0xce, 0xc1,
    0xb5, 0x6f, 0xe9, 0x1b, 0x32, 0x91, 0x34, 0x38
};

const base::FilePath::CharType kTestExtensionDir[] =
    FILE_PATH_LITERAL("extensions/api_test/enterprise_platform_keys");
const base::FilePath::CharType kUpdateManifestFileName[] =
    FILE_PATH_LITERAL("update_manifest.xml");

void ImportPrivateKeyPKCS8ToSlot(const unsigned char* pkcs8_der,
                                 size_t pkcs8_der_size,
                                 PK11SlotInfo* slot) {
  SECItem pki_der_user = {
      siBuffer,
      // NSS requires non-const data even though it is just for input.
      const_cast<unsigned char*>(pkcs8_der),
      pkcs8_der_size};

  SECKEYPrivateKey* seckey = NULL;
  ASSERT_EQ(SECSuccess,
            PK11_ImportDERPrivateKeyInfoAndReturnKey(slot,
                                                     &pki_der_user,
                                                     NULL,    // nickname
                                                     NULL,    // publicValue
                                                     true,    // isPerm
                                                     true,    // isPrivate
                                                     KU_ALL,  // usage
                                                     &seckey,
                                                     NULL));
}

// The managed_storage extension has a key defined in its manifest, so that
// its extension ID is well-known and the policy system can push policies for
// the extension.
const char kTestExtensionID[] = "aecpbnckhoppanpmefllkdkohionpmig";

enum SystemToken {
  SYSTEM_TOKEN_EXISTS,
  SYSTEM_TOKEN_NOT_EXISTS
};

enum DeviceStatus {
  DEVICE_STATUS_ENROLLED,
  DEVICE_STATUS_NOT_ENROLLED
};

enum UserAffiliation {
  USER_AFFILIATION_ENROLLED_DOMAIN,
  USER_AFFILIATION_UNRELATED
};

struct Params {
  Params(SystemToken system_token,
         DeviceStatus device_status,
         UserAffiliation user_affiliation)
      : system_token_(system_token),
        device_status_(device_status),
        user_affiliation_(user_affiliation) {}

  SystemToken system_token_;
  DeviceStatus device_status_;
  UserAffiliation user_affiliation_;
};

class EnterprisePlatformKeysTest
    : public ExtensionApiTest,
      public ::testing::WithParamInterface<Params> {
 public:
  EnterprisePlatformKeysTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);

    // Enable the WebCrypto API.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    std::string user_email = "someuser@anydomain.com";

    // The command line flag kLoginUser determines the user's email and thus
    // his affiliation to the domain that the device is enrolled to.
    if (GetParam().user_affiliation_ == USER_AFFILIATION_ENROLLED_DOMAIN)
      user_email = chromeos::login::kStubUser;

    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser, user_email);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    if (GetParam().device_status_ == DEVICE_STATUS_ENROLLED) {
      device_policy_test_helper_.device_policy()->policy_data().set_username(
          chromeos::login::kStubUser);

      device_policy_test_helper_.device_policy()->Build();
      device_policy_test_helper_.MarkAsEnterpriseOwned();
    }

    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    if (GetParam().system_token_ == SYSTEM_TOKEN_EXISTS) {
      base::RunLoop loop;
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(&EnterprisePlatformKeysTest::SetUpTestSystemSlotOnIO,
                     base::Unretained(this),
                     browser()->profile()->GetResourceContext(),
                     loop.QuitClosure()));
      loop.Run();
    }

    ExtensionApiTest::SetUpOnMainThread();

    // Enable the URLRequestMock, which is required for force-installing the
    // test extension through policy.
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(chrome_browser_net::SetUrlRequestMocksEnabled, true));

    {
      base::RunLoop loop;
      GetNSSCertDatabaseForProfile(
          browser()->profile(),
          base::Bind(&EnterprisePlatformKeysTest::DidGetCertDatabase,
                     base::Unretained(this),
                     loop.QuitClosure()));
      loop.Run();
    }

    SetPolicy();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    ExtensionApiTest::TearDownOnMainThread();

    if (GetParam().system_token_ == SYSTEM_TOKEN_EXISTS) {
      base::RunLoop loop;
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(&EnterprisePlatformKeysTest::TearDownTestSystemSlotOnIO,
                     base::Unretained(this),
                     loop.QuitClosure()));
      loop.Run();
    }
  }

 private:
  void DidGetCertDatabase(const base::Closure& done_callback,
                          net::NSSCertDatabase* cert_db) {
    // In order to use a prepared certificate, import a private key to the
    // user's token for which the Javscript test will import the certificate.
    ImportPrivateKeyPKCS8ToSlot(privateKeyPkcs8User,
                                arraysize(privateKeyPkcs8User),
                                cert_db->GetPrivateSlot().get());
    done_callback.Run();
  }

  void SetUpTestSystemSlotOnIO(content::ResourceContext* context,
                               const base::Closure& done_callback) {
    test_system_slot_.reset(new crypto::ScopedTestSystemNSSKeySlot());
    ASSERT_TRUE(test_system_slot_->ConstructedSuccessfully());

    // Import a private key to the system slot.  The Javascript part of this
    // test has a prepared certificate for this key.
    ImportPrivateKeyPKCS8ToSlot(privateKeyPkcs8System,
                                arraysize(privateKeyPkcs8System),
                                test_system_slot_->slot());

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE, done_callback);
  }

  void TearDownTestSystemSlotOnIO(const base::Closure& done_callback) {
    test_system_slot_.reset();

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE, done_callback);
  }

  void SetPolicy() {
    // Extensions that are force-installed come from an update URL, which
    // defaults to the webstore. Use a mock URL for this test with an update
    // manifest that includes the crx file of the test extension.
    base::FilePath update_manifest_path =
        base::FilePath(kTestExtensionDir).Append(kUpdateManifestFileName);
    GURL update_manifest_url(
        net::URLRequestMockHTTPJob::GetMockUrl(update_manifest_path));

    scoped_ptr<base::ListValue> forcelist(new base::ListValue);
    forcelist->AppendString(base::StringPrintf(
        "%s;%s", kTestExtensionID, update_manifest_url.spec().c_str()));

    policy::PolicyMap policy;
    policy.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_MACHINE,
               forcelist.release(),
               NULL);

    // Set the policy and wait until the extension is installed.
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
        content::NotificationService::AllSources());
    policy_provider_.UpdateChromePolicy(policy);
    observer.Wait();
  }

  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;
  scoped_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;
  policy::MockConfigurationPolicyProvider policy_provider_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(EnterprisePlatformKeysTest, Basic) {
  // By default, the system token is disabled.
  std::string system_token_availability = "";

  // Only if the system token exists, and the current user is of the same domain
  // as the device is enrolled to, the system token is available to the
  // extension.
  if (GetParam().system_token_ == SYSTEM_TOKEN_EXISTS &&
      GetParam().device_status_ == DEVICE_STATUS_ENROLLED &&
      GetParam().user_affiliation_ == USER_AFFILIATION_ENROLLED_DOMAIN) {
    system_token_availability = "systemTokenEnabled";
  }

  ASSERT_TRUE(RunExtensionSubtest(
      "",
      base::StringPrintf("chrome-extension://%s/basic.html?%s",
                         kTestExtensionID,
                         system_token_availability.c_str())))
      << message_;
}

INSTANTIATE_TEST_CASE_P(
    CheckSystemTokenAvailability,
    EnterprisePlatformKeysTest,
    ::testing::Values(Params(SYSTEM_TOKEN_EXISTS,
                             DEVICE_STATUS_ENROLLED,
                             USER_AFFILIATION_ENROLLED_DOMAIN),
                      Params(SYSTEM_TOKEN_EXISTS,
                             DEVICE_STATUS_ENROLLED,
                             USER_AFFILIATION_UNRELATED),
                      Params(SYSTEM_TOKEN_EXISTS,
                             DEVICE_STATUS_NOT_ENROLLED,
                             USER_AFFILIATION_UNRELATED),
                      Params(SYSTEM_TOKEN_NOT_EXISTS,
                             DEVICE_STATUS_ENROLLED,
                             USER_AFFILIATION_ENROLLED_DOMAIN)));

class EnterprisePlatformKeysTestNonPolicyInstalledExtension
    : public EnterprisePlatformKeysTest {};

// Ensure that extensions that are not pre-installed by policy throw an install
// warning if they request the enterprise.platformKeys permission in the
// manifest and that such extensions don't see the
// chrome.enterprise.platformKeys namespace.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       EnterprisePlatformKeysIsRestrictedToPolicyExtension) {
  ASSERT_TRUE(RunExtensionSubtest("enterprise_platform_keys",
                                  "api_not_available.html",
                                  kFlagIgnoreManifestWarnings));

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("enterprise_platform_keys");
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  const extensions::Extension* extension =
      GetExtensionByPath(service->extensions(), extension_path);
  ASSERT_FALSE(extension->install_warnings().empty());
  EXPECT_EQ(
      "'enterprise.platformKeys' is not allowed for specified install "
      "location.",
      extension->install_warnings()[0].message);
}
