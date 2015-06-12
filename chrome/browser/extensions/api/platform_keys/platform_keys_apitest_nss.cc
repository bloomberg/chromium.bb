// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/net/nss_context.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/user_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/test_root_certs.h"
#include "net/test/cert_test_util.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

enum DeviceStatus { DEVICE_STATUS_ENROLLED, DEVICE_STATUS_NOT_ENROLLED };

enum UserAffiliation {
  USER_AFFILIATION_ENROLLED_DOMAIN,
  USER_AFFILIATION_UNRELATED
};

struct Params {
  Params(DeviceStatus device_status, UserAffiliation user_affiliation)
      : device_status_(device_status), user_affiliation_(user_affiliation) {}

  DeviceStatus device_status_;
  UserAffiliation user_affiliation_;
};

class PlatformKeysTest : public ExtensionApiTest,
                         public ::testing::WithParamInterface<Params> {
 public:
  PlatformKeysTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);

    std::string user_email = "someuser@anydomain.com";

    // The command line flag kLoginUser determines the user's email and thus
    // his affiliation to the domain that the device is enrolled to.
    if (GetParam().user_affiliation_ == USER_AFFILIATION_ENROLLED_DOMAIN)
      user_email = chromeos::login::kStubUser;

    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser, user_email);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    if (GetParam().device_status_ == DEVICE_STATUS_ENROLLED) {
      device_policy_test_helper_.device_policy()->policy_data().set_username(
          chromeos::login::kStubUser);

      device_policy_test_helper_.device_policy()->Build();
      device_policy_test_helper_.MarkAsEnterpriseOwned();
    }
  }

  void SetUpOnMainThread() override {
    {
      base::RunLoop loop;
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::Bind(&PlatformKeysTest::SetUpTestSystemSlotOnIO,
                     base::Unretained(this),
                     browser()->profile()->GetResourceContext(),
                     loop.QuitClosure()));
      loop.Run();
    }

    ExtensionApiTest::SetUpOnMainThread();

    {
      base::RunLoop loop;
      GetNSSCertDatabaseForProfile(
          browser()->profile(),
          base::Bind(&PlatformKeysTest::SetupTestCerts, base::Unretained(this),
                     loop.QuitClosure()));
      loop.Run();
    }

    base::FilePath extension_path = test_data_dir_.AppendASCII("platform_keys");
    extension_ = LoadExtension(extension_path);
  }

  void TearDownOnMainThread() override {
    ExtensionApiTest::TearDownOnMainThread();

    base::RunLoop loop;
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&PlatformKeysTest::TearDownTestSystemSlotOnIO,
                   base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  chromeos::PlatformKeysService* GetPlatformKeysService() {
    return chromeos::PlatformKeysServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

  bool RunExtensionTest(const std::string& test_suite_name) {
    // By default, the system token is not available.
    std::string system_token_availability;

    // Only if the current user is of the same domain as the device is enrolled
    // to, the system token is available to the extension.
    if (GetParam().device_status_ == DEVICE_STATUS_ENROLLED &&
        GetParam().user_affiliation_ == USER_AFFILIATION_ENROLLED_DOMAIN) {
      system_token_availability = "systemTokenEnabled";
    }

    GURL url = extension_->GetResourceURL(base::StringPrintf(
        "basic.html?%s#%s", system_token_availability.c_str(),
        test_suite_name.c_str()));
    return RunExtensionSubtest("platform_keys", url.spec());
  }

 protected:
  scoped_refptr<net::X509Certificate> client_cert1_;
  scoped_refptr<net::X509Certificate> client_cert2_;
  const extensions::Extension* extension_;

 private:
  void SetupTestCerts(const base::Closure& done_callback,
                      net::NSSCertDatabase* cert_db) {
    SetupTestClientCerts(cert_db);
    SetupTestCACerts();
    done_callback.Run();
  }

  void SetupTestClientCerts(net::NSSCertDatabase* cert_db) {
    client_cert1_ = net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8",
        cert_db->GetPrivateSlot().get());
    ASSERT_TRUE(client_cert1_.get());

    // Import a second client cert signed by another CA than client_1 into the
    // system wide key slot.
    client_cert2_ = net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), "client_2.pem", "client_2.pk8",
        test_system_slot_->slot());
    ASSERT_TRUE(client_cert2_.get());
  }

  void SetupTestCACerts() {
    net::TestRootCerts* root_certs = net::TestRootCerts::GetInstance();
    // "root_ca_cert.pem" is the issuer of "ok_cert.pem" which is loaded on the
    // JS side. Generated by create_test_certs.sh .
    base::FilePath extension_path = test_data_dir_.AppendASCII("platform_keys");
    root_certs->AddFromFile(
        test_data_dir_.AppendASCII("platform_keys").AppendASCII("root.pem"));
  }

  void SetUpTestSystemSlotOnIO(content::ResourceContext* context,
                               const base::Closure& done_callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    test_system_slot_.reset(new crypto::ScopedTestSystemNSSKeySlot());
    ASSERT_TRUE(test_system_slot_->ConstructedSuccessfully());

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     done_callback);
  }

  void TearDownTestSystemSlotOnIO(const base::Closure& done_callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    test_system_slot_.reset();

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     done_callback);
  }

  policy::DevicePolicyCrosTestHelper device_policy_test_helper_;
  scoped_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;
};

class TestSelectDelegate
    : public chromeos::PlatformKeysService::SelectDelegate {
 public:
  // On each Select call, selects the next entry in |cert_to_select| from back
  // to front. Once the first entry is reached, that one will be selected
  // repeatedly.
  // Entries of |certs_to_select| can be null in which case no certificate will
  // be selected.
  explicit TestSelectDelegate(net::CertificateList certs_to_select)
      : certs_to_select_(certs_to_select) {}

  ~TestSelectDelegate() override {}

  void Select(const std::string& extension_id,
              const net::CertificateList& certs,
              const CertificateSelectedCallback& callback,
              content::WebContents* web_contents,
              content::BrowserContext* context) override {
    ASSERT_TRUE(web_contents);
    ASSERT_TRUE(context);
    ASSERT_FALSE(certs_to_select_.empty());
    scoped_refptr<net::X509Certificate> selection;
    if (certs_to_select_.back()) {
      for (scoped_refptr<net::X509Certificate> cert : certs) {
        if (cert->Equals(certs_to_select_.back().get())) {
          selection = cert;
          break;
        }
      }
    }
    if (certs_to_select_.size() > 1)
      certs_to_select_.pop_back();
    callback.Run(selection);
  }

 private:
  net::CertificateList certs_to_select_;
};

}  // namespace

// At first interactively selects |client_cert1_| and |client_cert2_| to grant
// permissions and afterwards runs more basic tests.
// After the initial two interactive calls, the simulated user does not select
// any cert.
IN_PROC_BROWSER_TEST_P(PlatformKeysTest, Basic) {
  net::CertificateList certs;
  certs.push_back(nullptr);
  certs.push_back(client_cert2_);
  certs.push_back(client_cert1_);

  GetPlatformKeysService()->SetSelectDelegate(
      make_scoped_ptr(new TestSelectDelegate(certs)));

  ASSERT_TRUE(RunExtensionTest("basicTests")) << message_;
}

// On interactive calls, the simulated user always selects |client_cert1_| if
// matching.
IN_PROC_BROWSER_TEST_P(PlatformKeysTest, Permissions) {
  net::CertificateList certs;
  certs.push_back(client_cert1_);

  GetPlatformKeysService()->SetSelectDelegate(
      make_scoped_ptr(new TestSelectDelegate(certs)));

  ASSERT_TRUE(RunExtensionTest("permissionTests")) << message_;
}

INSTANTIATE_TEST_CASE_P(
    CheckSystemTokenAvailability,
    PlatformKeysTest,
    ::testing::Values(
        Params(DEVICE_STATUS_ENROLLED, USER_AFFILIATION_ENROLLED_DOMAIN),
        Params(DEVICE_STATUS_ENROLLED, USER_AFFILIATION_UNRELATED),
        Params(DEVICE_STATUS_NOT_ENROLLED, USER_AFFILIATION_UNRELATED)));
