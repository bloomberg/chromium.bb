// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/network/fake_network_device_handler.h"
#include "chromeos/network/mock_managed_network_configuration_handler.h"
#include "chromeos/network/onc/mock_certificate_importer.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/user_manager/user_type.h"
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
  virtual user_manager::UserType GetType() const OVERRIDE {
    return user_manager::USER_TYPE_REGULAR;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeUser);
};

class FakeWebTrustedCertsObserver
    : public UserNetworkConfigurationUpdater::WebTrustedCertsObserver {
 public:
  virtual void OnTrustAnchorsChanged(
      const net::CertificateList& trust_anchors) OVERRIDE {
    trust_anchors_ = trust_anchors;
  }
  net::CertificateList trust_anchors_;
};

class FakeNetworkDeviceHandler : public chromeos::FakeNetworkDeviceHandler {
  public:
   FakeNetworkDeviceHandler() : allow_roaming_(false) {}

   virtual void SetCellularAllowRoaming(bool allow_roaming) OVERRIDE {
     allow_roaming_ = allow_roaming;
   }

   bool allow_roaming_;
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
    "  \"GlobalNetworkConfiguration\": {"
    "    \"AllowOnlyPolicyNetworksToAutoconnect\": true,"
    "  },"
    "  \"Certificates\": ["
    "    { \"GUID\": \"{f998f760-272b-6939-4c2beffe428697ac}\","
    "      \"PKCS12\": \"abc\","
    "       \"Type\": \"Client\" }"
    "  ],"
    "  \"Type\": \"UnencryptedConfiguration\""
    "}";

std::string ValueToString(const base::Value& value) {
  std::stringstream str;
  str << value;
  return str.str();
}

void AppendAll(const base::ListValue& from, base::ListValue* to) {
  for (base::ListValue::const_iterator it = from.begin(); it != from.end();
       ++it) {
    to->Append((*it)->DeepCopy());
  }
}

// Matcher to match base::Value.
MATCHER_P(IsEqualTo,
          value,
          std::string(negation ? "isn't" : "is") + " equal to " +
              ValueToString(*value)) {
  return value->Equals(&arg);
}

MATCHER(IsEmpty, std::string(negation ? "isn't" : "is") + " empty.") {
  return arg.empty();
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
        .WillRepeatedly(Return(false));
    provider_.Init();
    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider_);
    policy_service_.reset(new PolicyServiceImpl(providers));

    scoped_ptr<base::DictionaryValue> fake_toplevel_onc =
        chromeos::onc::ReadDictionaryFromJson(kFakeONC);

    base::ListValue* network_configs = NULL;
    fake_toplevel_onc->GetListWithoutPathExpansion(
        onc::toplevel_config::kNetworkConfigurations, &network_configs);
    AppendAll(*network_configs, &fake_network_configs_);

    base::DictionaryValue* global_config = NULL;
    fake_toplevel_onc->GetDictionaryWithoutPathExpansion(
        onc::toplevel_config::kGlobalNetworkConfiguration, &global_config);
    fake_global_network_config_.MergeDictionary(global_config);

    base::ListValue* certs = NULL;
    fake_toplevel_onc->GetListWithoutPathExpansion(
        onc::toplevel_config::kCertificates, &certs);
    AppendAll(*certs, &fake_certificates_);

    certificate_importer_ =
        new StrictMock<chromeos::onc::MockCertificateImporter>();
    certificate_importer_owned_.reset(certificate_importer_);
  }

  virtual void TearDown() OVERRIDE {
    network_configuration_updater_.reset();
    provider_.Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void MarkPolicyProviderInitialized() {
    Mock::VerifyAndClearExpectations(&provider_);
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    provider_.SetAutoRefresh();
    provider_.RefreshPolicies();
    base::RunLoop().RunUntilIdle();
  }

  void UpdateProviderPolicy(const PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

  UserNetworkConfigurationUpdater*
  CreateNetworkConfigurationUpdaterForUserPolicy(
      bool allow_trusted_certs_from_policy,
      bool set_cert_importer) {
    UserNetworkConfigurationUpdater* updater =
        UserNetworkConfigurationUpdater::CreateForUserPolicy(
            &profile_,
            allow_trusted_certs_from_policy,
            fake_user_,
            policy_service_.get(),
            &network_config_handler_).release();
    if (set_cert_importer) {
      EXPECT_TRUE(certificate_importer_owned_);
      updater->SetCertificateImporterForTest(
          certificate_importer_owned_.Pass());
    }
    network_configuration_updater_.reset(updater);
    return updater;
  }

  void CreateNetworkConfigurationUpdaterForDevicePolicy() {
    network_configuration_updater_ =
        DeviceNetworkConfigurationUpdater::CreateForDevicePolicy(
            policy_service_.get(),
            &network_config_handler_,
            &network_device_handler_,
            chromeos::CrosSettings::Get());
  }

  base::ListValue fake_network_configs_;
  base::DictionaryValue fake_global_network_config_;
  base::ListValue fake_certificates_;
  StrictMock<chromeos::MockManagedNetworkConfigurationHandler>
      network_config_handler_;
  FakeNetworkDeviceHandler network_device_handler_;

  // Not used directly. Required for CrosSettings.
  chromeos::ScopedTestDeviceSettingsService scoped_device_settings_service_;
  chromeos::ScopedTestCrosSettings scoped_cros_settings_;

  // Ownership of certificate_importer_owned_ is passed to the
  // NetworkConfigurationUpdater. When that happens, |certificate_importer_|
  // continues to point to that instance but |certificate_importer_owned_| is
  // released.
  StrictMock<chromeos::onc::MockCertificateImporter>* certificate_importer_;
  scoped_ptr<chromeos::onc::CertificateImporter> certificate_importer_owned_;

  StrictMock<MockConfigurationPolicyProvider> provider_;
  scoped_ptr<PolicyServiceImpl> policy_service_;
  FakeUser fake_user_;

  TestingProfile profile_;

  scoped_ptr<NetworkConfigurationUpdater> network_configuration_updater_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(NetworkConfigurationUpdaterTest, CellularAllowRoaming) {
  // Ignore network config updates.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _)).Times(AtLeast(1));

  // Setup the DataRoaming device setting.
  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
  chromeos::CrosSettingsProvider* device_settings_provider =
      cros_settings->GetProvider(chromeos::kSignedDataRoamingEnabled);
  cros_settings->RemoveSettingsProvider(device_settings_provider);
  delete device_settings_provider;
  chromeos::StubCrosSettingsProvider* stub_settings_provider =
      new chromeos::StubCrosSettingsProvider;
  cros_settings->AddSettingsProvider(stub_settings_provider);

  chromeos::CrosSettings::Get()->Set(chromeos::kSignedDataRoamingEnabled,
                                     base::FundamentalValue(false));
  EXPECT_FALSE(network_device_handler_.allow_roaming_);

  CreateNetworkConfigurationUpdaterForDevicePolicy();
  MarkPolicyProviderInitialized();
  chromeos::CrosSettings::Get()->Set(chromeos::kSignedDataRoamingEnabled,
                                     base::FundamentalValue(true));
  EXPECT_TRUE(network_device_handler_.allow_roaming_);

  chromeos::CrosSettings::Get()->Set(chromeos::kSignedDataRoamingEnabled,
                                     base::FundamentalValue(false));
  EXPECT_FALSE(network_device_handler_.allow_roaming_);
}

TEST_F(NetworkConfigurationUpdaterTest, PolicyIsValidatedAndRepaired) {
  scoped_ptr<base::DictionaryValue> onc_repaired =
      chromeos::onc::test_utils::ReadTestDictionary(
          "repaired_toplevel_partially_invalid.onc");

  base::ListValue* network_configs_repaired = NULL;
  onc_repaired->GetListWithoutPathExpansion(
      onc::toplevel_config::kNetworkConfigurations, &network_configs_repaired);
  ASSERT_TRUE(network_configs_repaired);

  base::DictionaryValue* global_config_repaired = NULL;
  onc_repaired->GetDictionaryWithoutPathExpansion(
      onc::toplevel_config::kGlobalNetworkConfiguration,
      &global_config_repaired);
  ASSERT_TRUE(global_config_repaired);

  std::string onc_policy =
      chromeos::onc::test_utils::ReadTestData("toplevel_partially_invalid.onc");
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(onc_policy),
             NULL);
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_config_handler_,
              SetPolicy(onc::ONC_SOURCE_USER_POLICY,
                        _,
                        IsEqualTo(network_configs_repaired),
                        IsEqualTo(global_config_repaired)));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(_, onc::ONC_SOURCE_USER_POLICY,  _));

  CreateNetworkConfigurationUpdaterForUserPolicy(
      false /* do not allow trusted certs from policy */,
      true /* set certificate importer */);
  MarkPolicyProviderInitialized();
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
              SetPolicy(onc::ONC_SOURCE_USER_POLICY, _, _, _));
  EXPECT_CALL(*certificate_importer_, ImportCertificates(_, _, _))
      .WillRepeatedly(SetCertificateList(cert_list));

  UserNetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy(
          false /* do not allow trusted certs from policy */,
          true /* set certificate importer */);
  MarkPolicyProviderInitialized();

  // Certificates with the "Web" trust flag set should not be forwarded to
  // observers.
  FakeWebTrustedCertsObserver observer;
  updater->AddTrustedCertsObserver(&observer);

  base::RunLoop().RunUntilIdle();

  net::CertificateList trust_anchors;
  updater->GetWebTrustedCertificates(&trust_anchors);
  EXPECT_TRUE(trust_anchors.empty());

  EXPECT_TRUE(observer.trust_anchors_.empty());
  updater->RemoveTrustedCertsObserver(&observer);
}

TEST_F(NetworkConfigurationUpdaterTest,
       AllowTrustedCertificatesFromPolicyInitially) {
  // Ignore network configuration changes.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _))
      .Times(AnyNumber());

  net::CertificateList cert_list;
  cert_list =
      net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                         "ok_cert.pem",
                                         net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, cert_list.size());

  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(_, onc::ONC_SOURCE_USER_POLICY, _))
      .WillRepeatedly(SetCertificateList(cert_list));

  UserNetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy(
          true /* allow trusted certs from policy */,
          true /* set certificate importer */);
  MarkPolicyProviderInitialized();

  base::RunLoop().RunUntilIdle();

  // Certificates with the "Web" trust flag set will be returned.
  net::CertificateList trust_anchors;
  updater->GetWebTrustedCertificates(&trust_anchors);
  EXPECT_EQ(1u, trust_anchors.size());
}

TEST_F(NetworkConfigurationUpdaterTest,
       AllowTrustedCertificatesFromPolicyOnUpdate) {
  // Ignore network configuration changes.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _))
      .Times(AnyNumber());

  // Start with an empty certificate list.
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(_, onc::ONC_SOURCE_USER_POLICY, _))
      .WillRepeatedly(SetCertificateList(net::CertificateList()));

  UserNetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy(
          true /* allow trusted certs from policy */,
          true /* set certificate importer */);
  MarkPolicyProviderInitialized();

  FakeWebTrustedCertsObserver observer;
  updater->AddTrustedCertsObserver(&observer);

  base::RunLoop().RunUntilIdle();

  // Verify that the returned certificate list is empty.
  Mock::VerifyAndClearExpectations(certificate_importer_);
  {
    net::CertificateList trust_anchors;
    updater->GetWebTrustedCertificates(&trust_anchors);
    EXPECT_TRUE(trust_anchors.empty());
  }
  EXPECT_TRUE(observer.trust_anchors_.empty());

  // Now use a non-empty certificate list to test the observer notification.
  net::CertificateList cert_list;
  cert_list =
      net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                         "ok_cert.pem",
                                         net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, cert_list.size());

  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(_, onc::ONC_SOURCE_USER_POLICY, _))
      .WillOnce(SetCertificateList(cert_list));

  // Change to any non-empty policy, so that updates are triggered. The actual
  // content of the policy is irrelevant.
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(kFakeONC),
             NULL);
  UpdateProviderPolicy(policy);
  base::RunLoop().RunUntilIdle();

  // Certificates with the "Web" trust flag set will be returned and forwarded
  // to observers.
  {
    net::CertificateList trust_anchors;
    updater->GetWebTrustedCertificates(&trust_anchors);
    EXPECT_EQ(1u, trust_anchors.size());
  }
  EXPECT_EQ(1u, observer.trust_anchors_.size());

  updater->RemoveTrustedCertsObserver(&observer);
}

TEST_F(NetworkConfigurationUpdaterTest,
       DontImportCertificateBeforeCertificateImporterSet) {
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, new base::StringValue(kFakeONC), NULL);
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_config_handler_,
              SetPolicy(onc::ONC_SOURCE_USER_POLICY,
                        kFakeUsernameHash,
                        IsEqualTo(&fake_network_configs_),
                        IsEqualTo(&fake_global_network_config_)));
  EXPECT_CALL(*certificate_importer_, ImportCertificates(_, _ , _)).Times(0);

  UserNetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy(
          true /* allow trusted certs from policy */,
          false /* do not set certificate importer */);
  MarkPolicyProviderInitialized();

  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(certificate_importer_);

  EXPECT_CALL(network_config_handler_,
              SetPolicy(onc::ONC_SOURCE_USER_POLICY,
                        kFakeUsernameHash,
                        IsEqualTo(&fake_network_configs_),
                        IsEqualTo(&fake_global_network_config_)))
      .Times(0);
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(IsEqualTo(&fake_certificates_),
                                 onc::ONC_SOURCE_USER_POLICY,
                                  _));

  ASSERT_TRUE(certificate_importer_owned_);
  updater->SetCertificateImporterForTest(certificate_importer_owned_.Pass());
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

  size_t ExpectedImportCertificatesCallCount() {
    if (GetParam() == key::kOpenNetworkConfiguration)
      return 1u;
    return 0u;
  }

  void CreateNetworkConfigurationUpdater() {
    if (GetParam() == key::kOpenNetworkConfiguration) {
      CreateNetworkConfigurationUpdaterForUserPolicy(
          false /* do not allow trusted certs from policy */,
          true /* set certificate importer */);
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
                        IsEqualTo(&fake_network_configs_),
                        IsEqualTo(&fake_global_network_config_)));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(IsEqualTo(&fake_certificates_),
                                 CurrentONCSource(),
                                  _))
      .Times(ExpectedImportCertificatesCallCount());

  CreateNetworkConfigurationUpdater();
  MarkPolicyProviderInitialized();
}

TEST_P(NetworkConfigurationUpdaterTestWithParam,
       PolicyNotSetBeforePolicyProviderInitialized) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             new base::StringValue(kFakeONC), NULL);
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_config_handler_,
              SetPolicy(CurrentONCSource(),
                        ExpectedUsernameHash(),
                        IsEqualTo(&fake_network_configs_),
                        IsEqualTo(&fake_global_network_config_)))
      .Times(0);
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(IsEqualTo(&fake_certificates_),
                                 CurrentONCSource(),
                                  _))
      .Times(0);

  CreateNetworkConfigurationUpdater();

  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(certificate_importer_);

  EXPECT_CALL(network_config_handler_,
              SetPolicy(CurrentONCSource(),
                        ExpectedUsernameHash(),
                        IsEqualTo(&fake_network_configs_),
                        IsEqualTo(&fake_global_network_config_)));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(IsEqualTo(&fake_certificates_),
                                 CurrentONCSource(),
                                  _))
      .Times(ExpectedImportCertificatesCallCount());

  MarkPolicyProviderInitialized();
}

TEST_P(NetworkConfigurationUpdaterTestWithParam,
       PolicyAppliedImmediatelyIfProvidersInitialized) {
  MarkPolicyProviderInitialized();

  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             new base::StringValue(kFakeONC), NULL);
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_config_handler_,
              SetPolicy(CurrentONCSource(),
                        ExpectedUsernameHash(),
                        IsEqualTo(&fake_network_configs_),
                        IsEqualTo(&fake_global_network_config_)));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(IsEqualTo(&fake_certificates_),
                                 CurrentONCSource(),
                                  _))
      .Times(ExpectedImportCertificatesCallCount());

  CreateNetworkConfigurationUpdater();
}

TEST_P(NetworkConfigurationUpdaterTestWithParam, PolicyChange) {
  // Ignore the initial updates.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(*certificate_importer_, ImportCertificates(_, _, _))
      .Times(AtLeast(ExpectedImportCertificatesCallCount()));

  CreateNetworkConfigurationUpdater();
  MarkPolicyProviderInitialized();

  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(certificate_importer_);

  // The Updater should update if policy changes.
  EXPECT_CALL(network_config_handler_,
              SetPolicy(CurrentONCSource(),
                        _,
                        IsEqualTo(&fake_network_configs_),
                        IsEqualTo(&fake_global_network_config_)));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(IsEqualTo(&fake_certificates_),
                                 CurrentONCSource(),
                                  _))
      .Times(ExpectedImportCertificatesCallCount());

  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             new base::StringValue(kFakeONC), NULL);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  Mock::VerifyAndClearExpectations(certificate_importer_);

  // Another update is expected if the policy goes away.
  EXPECT_CALL(network_config_handler_,
              SetPolicy(CurrentONCSource(), _, IsEmpty(), IsEmpty()));
  EXPECT_CALL(*certificate_importer_,
              ImportCertificates(IsEmpty(), CurrentONCSource(), _))
      .Times(ExpectedImportCertificatesCallCount());

  policy.Erase(GetParam());
  UpdateProviderPolicy(policy);
}

INSTANTIATE_TEST_CASE_P(NetworkConfigurationUpdaterTestWithParamInstance,
                        NetworkConfigurationUpdaterTestWithParam,
                        testing::Values(key::kDeviceOpenNetworkConfiguration,
                                        key::kOpenNetworkConfiguration));

}  // namespace policy
