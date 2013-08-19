// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater_impl.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chromeos/network/mock_managed_network_configuration_handler.h"
#include "chromeos/network/onc/mock_certificate_importer.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_trust_anchor_provider.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::Mock;
using testing::Ne;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace onc = ::chromeos::onc;

namespace policy {

namespace {
const char kFakeUserEmail[] = "fake email";
const char kFakeUsernameHash[] = "fake hash";

class FakeUser : public chromeos::User {
 public:
  FakeUser() : User(kFakeUserEmail) {
    set_display_email(kFakeUserEmail);
    set_username_hash(kFakeUsernameHash);
  }
  virtual ~FakeUser() {}

  // User overrides
  virtual UserType GetType() const OVERRIDE {
    return USER_TYPE_REGULAR;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeUser);
};

const char kFakeONC[] =
    "{ \"NetworkConfigurations\": ["
    "    { \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5040}\","
    "      \"Type\": \"WiFi\","
    "      \"Name\": \"My WiFi Network\","
    "      \"WiFi\": {"
    "        \"SSID\": \"ssid-none\","
    "        \"Security\": \"None\" }"
    "    }"
    "  ],"
    "  \"Certificates\": ["
    "    { \"GUID\": \"{f998f760-272b-6939-4c2beffe428697ac}\","
    "      \"PKCS12\": \"abc\","
    "       \"Type\": \"Client\" }"
    "  ],"
    "  \"Type\": \"UnencryptedConfiguration\""
    "}";

std::string ValueToString(const base::Value* value) {
    std::stringstream str;
    str << *value;
    return str.str();
}

// Matcher to match base::Value.
MATCHER_P(IsEqualTo,
          value,
          std::string(negation ? "isn't" : "is") + " equal to " +
          ValueToString(value)) {
  return value->Equals(&arg);
}

ACTION_P(SetCertificateList, list) {
  *arg2 = list;
  return true;
}

ACTION_P(SetImportedCerts, map) {
  *arg3 = map;
  return true;
}

}  // namespace

class NetworkConfigurationUpdaterTest : public testing::Test {
 protected:
  NetworkConfigurationUpdaterTest()
      : ui_thread_(content::BrowserThread::UI, &loop_),
        io_thread_(content::BrowserThread::IO, &loop_) {}

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    provider_.Init();
    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider_);
    policy_service_.reset(new PolicyServiceImpl(providers));

    empty_network_configs_.reset(new base::ListValue);
    empty_certificates_.reset(new base::ListValue);

    scoped_ptr<base::DictionaryValue> fake_toplevel_onc =
        onc::ReadDictionaryFromJson(kFakeONC);

    scoped_ptr<base::Value> network_configs_value;
    base::ListValue* network_configs = NULL;
    fake_toplevel_onc->RemoveWithoutPathExpansion(
        onc::toplevel_config::kNetworkConfigurations, &network_configs_value);
    network_configs_value.release()->GetAsList(&network_configs);
    fake_network_configs_.reset(network_configs);

    scoped_ptr<base::Value> certs_value;
    base::ListValue* certs = NULL;
    fake_toplevel_onc->RemoveWithoutPathExpansion(
        onc::toplevel_config::kCertificates, &certs_value);
    certs_value.release()->GetAsList(&certs);
    fake_certificates_.reset(certs);
  }

  virtual void TearDown() OVERRIDE {
    provider_.Shutdown();
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  }

  void UpdateProviderPolicy(const PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  scoped_ptr<base::ListValue> empty_network_configs_;
  scoped_ptr<base::ListValue> empty_certificates_;
  scoped_ptr<base::ListValue> fake_network_configs_;
  scoped_ptr<base::ListValue> fake_certificates_;
  StrictMock<chromeos::MockManagedNetworkConfigurationHandler>
      network_config_handler_;
  StrictMock<MockConfigurationPolicyProvider> provider_;
  scoped_ptr<PolicyServiceImpl> policy_service_;
  FakeUser fake_user_;
  base::MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

TEST_F(NetworkConfigurationUpdaterTest, PolicyIsValidatedAndRepaired) {
  std::string onc_policy =
      onc::test_utils::ReadTestData("toplevel_partially_invalid.onc");

  scoped_ptr<base::DictionaryValue> onc_repaired =
      onc::test_utils::ReadTestDictionary(
          "repaired_toplevel_partially_invalid.onc");

  base::ListValue* network_configs_repaired = NULL;
  onc_repaired->GetListWithoutPathExpansion(
      onc::toplevel_config::kNetworkConfigurations, &network_configs_repaired);
  ASSERT_TRUE(network_configs_repaired);

  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, Value::CreateStringValue(onc_policy), NULL);
  UpdateProviderPolicy(policy);

  // Ignore the device policy update.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _));
  StrictMock<chromeos::onc::MockCertificateImporter>* certificate_importer =
      new StrictMock<chromeos::onc::MockCertificateImporter>();
  EXPECT_CALL(*certificate_importer, ImportCertificates(_, _, _));

  NetworkConfigurationUpdaterImpl updater(
      policy_service_.get(),
      &network_config_handler_,
      make_scoped_ptr<chromeos::onc::CertificateImporter>(
          certificate_importer));
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(&certificate_importer);

  // After the user policy is initialized.
  EXPECT_CALL(
      network_config_handler_,
      SetPolicy(
          onc::ONC_SOURCE_USER_POLICY, _, IsEqualTo(network_configs_repaired)));
  EXPECT_CALL(*certificate_importer,
              ImportCertificates(_, chromeos::onc::ONC_SOURCE_USER_POLICY, _));

  updater.SetUserPolicyService(false, &fake_user_, policy_service_.get());
  updater.UnsetUserPolicyService();
}

class NetworkConfigurationUpdaterTestWithParam
    : public NetworkConfigurationUpdaterTest,
      public testing::WithParamInterface<const char*> {
 public:
  // Returns the currently tested ONC source.
  onc::ONCSource CurrentONCSource() {
    if (GetParam() == key::kDeviceOpenNetworkConfiguration)
      return onc::ONC_SOURCE_DEVICE_POLICY;
    if (GetParam() == key::kOpenNetworkConfiguration)
      return onc::ONC_SOURCE_USER_POLICY;
    return onc::ONC_SOURCE_NONE;
  }
};

TEST_P(NetworkConfigurationUpdaterTestWithParam, InitialUpdates) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             Value::CreateStringValue(kFakeONC), NULL);
  UpdateProviderPolicy(policy);

  // Initially, only the device policy is applied. The user policy is only
  // applied after the user profile was initialized.
  base::ListValue* device_networks;
  base::ListValue* device_certs;
  base::ListValue* user_networks;
  base::ListValue* user_certs;
  if (GetParam() == key::kDeviceOpenNetworkConfiguration) {
    device_networks = fake_network_configs_.get();
    device_certs = fake_certificates_.get();
    user_networks = empty_network_configs_.get();
    user_certs = empty_certificates_.get();
  } else {
    device_networks = empty_network_configs_.get();
    device_certs = empty_certificates_.get();
    user_networks = fake_network_configs_.get();
    user_certs = fake_certificates_.get();
  }

  EXPECT_CALL(
      network_config_handler_,
      SetPolicy(onc::ONC_SOURCE_DEVICE_POLICY, _, IsEqualTo(device_networks)));
  StrictMock<chromeos::onc::MockCertificateImporter>* certificate_importer =
      new StrictMock<chromeos::onc::MockCertificateImporter>();
  EXPECT_CALL(*certificate_importer, ImportCertificates(
      IsEqualTo(device_certs), onc::ONC_SOURCE_DEVICE_POLICY, _));

  NetworkConfigurationUpdaterImpl updater(
      policy_service_.get(),
      &network_config_handler_,
      make_scoped_ptr<chromeos::onc::CertificateImporter>(
          certificate_importer));
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(&certificate_importer);

  EXPECT_CALL(
      network_config_handler_,
      SetPolicy(onc::ONC_SOURCE_USER_POLICY, _, IsEqualTo(user_networks)));
  EXPECT_CALL(*certificate_importer, ImportCertificates(
      IsEqualTo(user_certs), onc::ONC_SOURCE_USER_POLICY, _));

  // We just need an initialized PolicyService, so we can reuse
  // |policy_service_|.
  updater.SetUserPolicyService(false, &fake_user_, policy_service_.get());
  updater.UnsetUserPolicyService();
}

TEST_P(NetworkConfigurationUpdaterTestWithParam,
       AllowTrustedCertificatesFromPolicy) {
  const net::CertificateList empty_cert_list;

  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _)).Times(AnyNumber());
  StrictMock<chromeos::onc::MockCertificateImporter>* certificate_importer =
      new StrictMock<chromeos::onc::MockCertificateImporter>();
  EXPECT_CALL(*certificate_importer, ImportCertificates(_, _, _))
      .WillRepeatedly(SetCertificateList(empty_cert_list));
  NetworkConfigurationUpdaterImpl updater(
      policy_service_.get(),
      &network_config_handler_,
      make_scoped_ptr<chromeos::onc::CertificateImporter>(
          certificate_importer));
  net::CertTrustAnchorProvider* trust_provider =
      updater.GetCertTrustAnchorProvider();
  ASSERT_TRUE(trust_provider);
  // The initial list of trust anchors is empty.
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  EXPECT_TRUE(trust_provider->GetAdditionalTrustAnchors().empty());

  // Initially, certificates imported from policy don't have trust flags.
  updater.SetUserPolicyService(false, &fake_user_, policy_service_.get());
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(&certificate_importer);
  EXPECT_TRUE(trust_provider->GetAdditionalTrustAnchors().empty());

  // Certificates with the "Web" trust flag set should be forwarded to the
  // trust provider.
  EXPECT_CALL(network_config_handler_,
              SetPolicy(chromeos::onc::ONC_SOURCE_USER_POLICY, _, _));

  net::CertificateList cert_list;
  if (GetParam() == key::kOpenNetworkConfiguration) {
    cert_list =
        net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                           "ok_cert.pem",
                                           net::X509Certificate::FORMAT_AUTO);
    ASSERT_EQ(1u, cert_list.size());
  }
  EXPECT_CALL(*certificate_importer,
              ImportCertificates(_, chromeos::onc::ONC_SOURCE_USER_POLICY, _))
      .WillRepeatedly(SetCertificateList(cert_list));

  // Trigger a new policy load, and spin the IO message loop to pass the
  // certificates to the |trust_provider| on the IO thread.
  updater.SetUserPolicyService(true, &fake_user_, policy_service_.get());
  base::RunLoop loop;
  loop.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(&certificate_importer);

  // Certificates are only provided as trust anchors if they come from user
  // policy.
  size_t expected_certs = 0u;
  if (GetParam() == key::kOpenNetworkConfiguration)
    expected_certs = 1u;
  EXPECT_EQ(expected_certs,
            trust_provider->GetAdditionalTrustAnchors().size());

  updater.UnsetUserPolicyService();
}

TEST_P(NetworkConfigurationUpdaterTestWithParam, PolicyChange) {
  // Ignore the initial updates.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _)).Times(AnyNumber());
  StrictMock<chromeos::onc::MockCertificateImporter>* certificate_importer =
      new StrictMock<chromeos::onc::MockCertificateImporter>();
  EXPECT_CALL(*certificate_importer, ImportCertificates(_, _, _))
      .Times(AnyNumber());
  NetworkConfigurationUpdaterImpl updater(
      policy_service_.get(),
      &network_config_handler_,
      make_scoped_ptr<chromeos::onc::CertificateImporter>(
          certificate_importer));
  updater.SetUserPolicyService(false, &fake_user_, policy_service_.get());
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(&certificate_importer);

  // We should update if policy changes.
  EXPECT_CALL(network_config_handler_,
              SetPolicy(CurrentONCSource(),
                        _,
                        IsEqualTo(fake_network_configs_.get())));
  EXPECT_CALL(*certificate_importer, ImportCertificates(
      IsEqualTo(fake_certificates_.get()), CurrentONCSource(), _));

  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             Value::CreateStringValue(kFakeONC), NULL);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(&certificate_importer);

  // Another update is expected if the policy goes away.
  EXPECT_CALL(network_config_handler_,
              SetPolicy(CurrentONCSource(),
                        _,
                        IsEqualTo(empty_network_configs_.get())));
  EXPECT_CALL(*certificate_importer, ImportCertificates(
      IsEqualTo(empty_certificates_.get()),
      CurrentONCSource(),
      _));

  policy.Erase(GetParam());
  UpdateProviderPolicy(policy);
  updater.UnsetUserPolicyService();
}

INSTANTIATE_TEST_CASE_P(
    NetworkConfigurationUpdaterTestWithParamInstance,
    NetworkConfigurationUpdaterTestWithParam,
    testing::Values(key::kDeviceOpenNetworkConfiguration,
                    key::kOpenNetworkConfiguration));

}  // namespace policy
