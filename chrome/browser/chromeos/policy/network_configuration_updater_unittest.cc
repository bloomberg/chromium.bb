// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/policy/policy_cert_verifier.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chromeos/network/mock_managed_network_configuration_handler.h"
#include "chromeos/network/onc/mock_certificate_importer.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::AtLeast;
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
  if (arg2)
    *arg2 = list;
  return true;
}

}  // namespace

class NetworkConfigurationUpdaterTest : public testing::Test {
 protected:
  NetworkConfigurationUpdaterTest() {
  }

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

    certificate_importer_ = new StrictMock<onc::MockCertificateImporter>();
    certificate_importer_owned_.reset(certificate_importer_);
  }

  virtual void TearDown() OVERRIDE {
    network_configuration_updater_.reset();
    provider_.Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void UpdateProviderPolicy(const PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

  UserNetworkConfigurationUpdater*
  CreateNetworkConfigurationUpdaterForUserPolicy(
      bool allow_trusted_certs_from_policy) {
    UserNetworkConfigurationUpdater* updater =
        UserNetworkConfigurationUpdater::CreateForUserPolicy(
            allow_trusted_certs_from_policy,
            fake_user_,
            certificate_importer_owned_.Pass(),
            policy_service_.get(),
            &network_config_handler_).release();
    network_configuration_updater_.reset(updater);
    return updater;
  }

  void CreateNetworkConfigurationUpdaterForDevicePolicy() {
    network_configuration_updater_ =
        NetworkConfigurationUpdater::CreateForDevicePolicy(
            certificate_importer_owned_.Pass(),
            policy_service_.get(),
            &network_config_handler_);
  }

  scoped_ptr<base::ListValue> empty_network_configs_;
  scoped_ptr<base::ListValue> empty_certificates_;
  scoped_ptr<base::ListValue> fake_network_configs_;
  scoped_ptr<base::ListValue> fake_certificates_;
  StrictMock<chromeos::MockManagedNetworkConfigurationHandler>
      network_config_handler_;

  // Ownership of certificate_importer_owned_ is passed to the
  // NetworkConfigurationUpdater. When that happens, |certificate_importer_|
  // continues to point to that instance but |certificate_importer_owned_| is
  // released.
  StrictMock<onc::MockCertificateImporter>* certificate_importer_;
  scoped_ptr<onc::CertificateImporter> certificate_importer_owned_;

  StrictMock<MockConfigurationPolicyProvider> provider_;
  scoped_ptr<PolicyServiceImpl> policy_service_;
  FakeUser fake_user_;

  scoped_ptr<NetworkConfigurationUpdater> network_configuration_updater_;
  content::TestBrowserThreadBundle thread_bundle_;
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
  policy.Set(key::kOpenNetworkConfiguration,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(onc_policy),
             NULL);
  UpdateProviderPolicy(policy);

  EXPECT_CALL(
      network_config_handler_,
      SetPolicy(
          onc::ONC_SOURCE_USER_POLICY, _, IsEqualTo(network_configs_repaired)));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(_, onc::ONC_SOURCE_USER_POLICY, _));

  CreateNetworkConfigurationUpdaterForUserPolicy(
      false /* do not allow trusted certs from policy */ );
}

TEST_F(NetworkConfigurationUpdaterTest,
       DoNotAllowTrustedCertificatesFromPolicy) {
  net::CertificateList cert_list;
  cert_list =
      net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                         "ok_cert.pem",
                                         net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, cert_list.size());

  EXPECT_CALL(network_config_handler_,
              SetPolicy(onc::ONC_SOURCE_USER_POLICY, _, _));
  EXPECT_CALL(*certificate_importer_, ImportCertificates(_, _, _))
      .WillRepeatedly(SetCertificateList(cert_list));

  UserNetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy(
          false /* do not allow trusted certs from policy */);

  // Certificates with the "Web" trust flag set should not be forwarded to the
  // trust provider.
  policy::PolicyCertVerifier cert_verifier((
      base::Closure() /* no policy cert trusted callback */));
  updater->SetPolicyCertVerifier(&cert_verifier);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(cert_verifier.GetAdditionalTrustAnchors().empty());

  // |cert_verifier| must outlive the updater.
  network_configuration_updater_.reset();
}

TEST_F(NetworkConfigurationUpdaterTest, AllowTrustedCertificatesFromPolicy) {
  net::CertificateList cert_list;
  cert_list =
      net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                         "ok_cert.pem",
                                         net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, cert_list.size());

  EXPECT_CALL(network_config_handler_,
              SetPolicy(onc::ONC_SOURCE_USER_POLICY, _, _));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(_, onc::ONC_SOURCE_USER_POLICY, _))
      .WillRepeatedly(SetCertificateList(cert_list));

  UserNetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy(
          true /* allow trusted certs from policy */);

  // Certificates with the "Web" trust flag set should be forwarded to the
  // trust provider.
  policy::PolicyCertVerifier cert_verifier((
      base::Closure() /* no policy cert trusted callback */));
  updater->SetPolicyCertVerifier(&cert_verifier);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, cert_verifier.GetAdditionalTrustAnchors().size());

  // |cert_verifier| must outlive the updater.
  network_configuration_updater_.reset();
}

class NetworkConfigurationUpdaterTestWithParam
    : public NetworkConfigurationUpdaterTest,
      public testing::WithParamInterface<const char*> {
 protected:
  // Returns the currently tested ONC source.
  onc::ONCSource CurrentONCSource() {
    if (GetParam() == key::kOpenNetworkConfiguration)
      return onc::ONC_SOURCE_USER_POLICY;
    DCHECK(GetParam() == key::kDeviceOpenNetworkConfiguration);
    return onc::ONC_SOURCE_DEVICE_POLICY;
  }

  // Returns the expected username hash to push policies to
  // ManagedNetworkConfigurationHandler.
  std::string ExpectedUsernameHash() {
    if (GetParam() == key::kOpenNetworkConfiguration)
      return kFakeUsernameHash;
    return std::string();
  }

  void CreateNetworkConfigurationUpdater() {
    if (GetParam() == key::kOpenNetworkConfiguration) {
      CreateNetworkConfigurationUpdaterForUserPolicy(
          false /* do not allow trusted certs from policy */);
    } else {
      CreateNetworkConfigurationUpdaterForDevicePolicy();
    }
  }
};

TEST_P(NetworkConfigurationUpdaterTestWithParam, InitialUpdates) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             new base::StringValue(kFakeONC), NULL);
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_config_handler_,
              SetPolicy(CurrentONCSource(),
                        ExpectedUsernameHash(),
                        IsEqualTo(fake_network_configs_.get())));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(
                  IsEqualTo(fake_certificates_.get()), CurrentONCSource(), _));

  CreateNetworkConfigurationUpdater();
}


TEST_P(NetworkConfigurationUpdaterTestWithParam, PolicyChange) {
  // Ignore the initial updates.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _)).Times(AtLeast(1));
  EXPECT_CALL(*certificate_importer_, ImportCertificates(_, _, _))
      .Times(AtLeast(1));
  CreateNetworkConfigurationUpdater();
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(certificate_importer_);

  // The Updater should update if policy changes.
  EXPECT_CALL(
      network_config_handler_,
      SetPolicy(CurrentONCSource(), _, IsEqualTo(fake_network_configs_.get())));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(
                  IsEqualTo(fake_certificates_.get()), CurrentONCSource(), _));

  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             new base::StringValue(kFakeONC), NULL);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(certificate_importer_);

  // Another update is expected if the policy goes away.
  EXPECT_CALL(
      network_config_handler_,
      SetPolicy(
          CurrentONCSource(), _, IsEqualTo(empty_network_configs_.get())));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(
                  IsEqualTo(empty_certificates_.get()), CurrentONCSource(), _));

  policy.Erase(GetParam());
  UpdateProviderPolicy(policy);
}

INSTANTIATE_TEST_CASE_P(NetworkConfigurationUpdaterTestWithParamInstance,
                        NetworkConfigurationUpdaterTestWithParam,
                        testing::Values(key::kDeviceOpenNetworkConfiguration,
                                        key::kOpenNetworkConfiguration));

}  // namespace policy
