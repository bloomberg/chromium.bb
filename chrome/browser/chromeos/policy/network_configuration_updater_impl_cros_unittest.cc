// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater_impl_cros.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chromeos/network/mock_certificate_handler.h"
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

}  // namespace

// Tests of NetworkConfigurationUpdaterImplCros
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

    base::Value* network_configs_value = NULL;
    base::ListValue* network_configs = NULL;
    fake_toplevel_onc->RemoveWithoutPathExpansion(
        onc::toplevel_config::kNetworkConfigurations,
        &network_configs_value);
    network_configs_value->GetAsList(&network_configs);
    fake_network_configs_.reset(network_configs);

    base::Value* certs_value = NULL;
    base::ListValue* certs = NULL;
    fake_toplevel_onc->RemoveWithoutPathExpansion(
        onc::toplevel_config::kCertificates,
        &certs_value);
    certs_value->GetAsList(&certs);
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

  // Maps configuration policy name to corresponding ONC source.
  static onc::ONCSource NameToONCSource(
      const std::string& name) {
    if (name == key::kDeviceOpenNetworkConfiguration)
      return onc::ONC_SOURCE_DEVICE_POLICY;
    if (name == key::kOpenNetworkConfiguration)
      return onc::ONC_SOURCE_USER_POLICY;
    return onc::ONC_SOURCE_NONE;
  }

  scoped_ptr<base::ListValue> empty_network_configs_;
  scoped_ptr<base::ListValue> empty_certificates_;
  scoped_ptr<base::ListValue> fake_network_configs_;
  scoped_ptr<base::ListValue> fake_certificates_;
  StrictMock<chromeos::MockNetworkLibrary> network_library_;
  StrictMock<MockConfigurationPolicyProvider> provider_;
  scoped_ptr<PolicyServiceImpl> policy_service_;
  MessageLoop loop_;
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
      onc::toplevel_config::kNetworkConfigurations,
      &network_configs_repaired);
  ASSERT_TRUE(network_configs_repaired);

  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, Value::CreateStringValue(onc_policy));
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_library_, AddNetworkProfileObserver(_));

  // Ignore the device policy update.
  EXPECT_CALL(network_library_, LoadOncNetworks(_, _));
  StrictMock<chromeos::MockCertificateHandler>* certificate_handler =
      new StrictMock<chromeos::MockCertificateHandler>();
  EXPECT_CALL(*certificate_handler, ImportCertificates(_, _, _));

  NetworkConfigurationUpdaterImplCros updater(
      policy_service_.get(),
      &network_library_,
      make_scoped_ptr<chromeos::CertificateHandler>(certificate_handler));
  Mock::VerifyAndClearExpectations(&network_library_);
  Mock::VerifyAndClearExpectations(&certificate_handler);

  // After the user policy is initialized, we always push both policies to the
  // NetworkLibrary. Ignore the device policy.
  EXPECT_CALL(network_library_, LoadOncNetworks(
      _, onc::ONC_SOURCE_DEVICE_POLICY));
  EXPECT_CALL(network_library_, LoadOncNetworks(
      IsEqualTo(network_configs_repaired),
      onc::ONC_SOURCE_USER_POLICY));
  EXPECT_CALL(*certificate_handler, ImportCertificates(_, _, _)).Times(2);

  EXPECT_CALL(network_library_, RemoveNetworkProfileObserver(_));

  updater.OnUserPolicyInitialized(false, "hash");
}

class NetworkConfigurationUpdaterTestWithParam
    : public NetworkConfigurationUpdaterTest,
      public testing::WithParamInterface<const char*> {
};

TEST_P(NetworkConfigurationUpdaterTestWithParam, InitialUpdates) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             Value::CreateStringValue(kFakeONC));
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_library_, AddNetworkProfileObserver(_));

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

  EXPECT_CALL(network_library_, LoadOncNetworks(
      IsEqualTo(device_networks), onc::ONC_SOURCE_DEVICE_POLICY));
  StrictMock<chromeos::MockCertificateHandler>* certificate_handler =
      new StrictMock<chromeos::MockCertificateHandler>();
  EXPECT_CALL(*certificate_handler, ImportCertificates(
      IsEqualTo(device_certs), onc::ONC_SOURCE_DEVICE_POLICY, _));

  NetworkConfigurationUpdaterImplCros updater(
      policy_service_.get(),
      &network_library_,
      make_scoped_ptr<chromeos::CertificateHandler>(certificate_handler));
  Mock::VerifyAndClearExpectations(&network_library_);
  Mock::VerifyAndClearExpectations(&certificate_handler);

  // After the user policy is initialized, we always push both policies to the
  // NetworkLibrary.
  EXPECT_CALL(network_library_, LoadOncNetworks(
      IsEqualTo(device_networks), onc::ONC_SOURCE_DEVICE_POLICY));
  EXPECT_CALL(*certificate_handler, ImportCertificates(
      IsEqualTo(device_certs), onc::ONC_SOURCE_DEVICE_POLICY, _));

  EXPECT_CALL(network_library_, LoadOncNetworks(
      IsEqualTo(user_networks), onc::ONC_SOURCE_USER_POLICY));
  EXPECT_CALL(*certificate_handler, ImportCertificates(
      IsEqualTo(user_certs), onc::ONC_SOURCE_USER_POLICY, _));

  EXPECT_CALL(network_library_, RemoveNetworkProfileObserver(_));

  updater.OnUserPolicyInitialized(false, "hash");
}

TEST_P(NetworkConfigurationUpdaterTestWithParam,
       AllowTrustedCertificatesFromPolicy) {
  EXPECT_CALL(network_library_, AddNetworkProfileObserver(_));

  const net::CertificateList empty_cert_list;

  const net::CertificateList cert_list =
      net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                         "ok_cert.pem",
                                         net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, cert_list.size());

  EXPECT_CALL(network_library_, LoadOncNetworks(_, _)).Times(AnyNumber());
  StrictMock<chromeos::MockCertificateHandler>* certificate_handler =
      new StrictMock<chromeos::MockCertificateHandler>();
  EXPECT_CALL(*certificate_handler, ImportCertificates(_, _, _))
      .WillRepeatedly(SetCertificateList(empty_cert_list));
  NetworkConfigurationUpdaterImplCros updater(
      policy_service_.get(),
      &network_library_,
      make_scoped_ptr<chromeos::CertificateHandler>(certificate_handler));
  net::CertTrustAnchorProvider* trust_provider =
      updater.GetCertTrustAnchorProvider();
  ASSERT_TRUE(trust_provider);
  // The initial list of trust anchors is empty.
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  EXPECT_TRUE(trust_provider->GetAdditionalTrustAnchors().empty());

  // Initially, certificates imported from policy don't have trust flags.
  updater.OnUserPolicyInitialized(false, "hash");
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  Mock::VerifyAndClearExpectations(&network_library_);
  Mock::VerifyAndClearExpectations(&certificate_handler);
  EXPECT_TRUE(trust_provider->GetAdditionalTrustAnchors().empty());

  // Certificates with the "Web" trust flag set should be forwarded to the
  // trust provider.
  EXPECT_CALL(network_library_, LoadOncNetworks(_, _));
  EXPECT_CALL(*certificate_handler, ImportCertificates(_, _, _))
      .WillRepeatedly(SetCertificateList(empty_cert_list));
  onc::ONCSource current_source = NameToONCSource(GetParam());
  EXPECT_CALL(network_library_, LoadOncNetworks(_, current_source));
  EXPECT_CALL(*certificate_handler, ImportCertificates(_, current_source, _))
      .WillRepeatedly(SetCertificateList(cert_list));
  // Trigger a new policy load, and spin the IO message loop to pass the
  // certificates to the |trust_provider| on the IO thread.
  updater.OnUserPolicyInitialized(true, "hash");
  base::RunLoop loop;
  loop.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&network_library_);
  Mock::VerifyAndClearExpectations(&certificate_handler);

  // Certificates are only provided as trust anchors if they come from user
  // policy.
  size_t expected_certs = 0u;
  if (GetParam() == key::kOpenNetworkConfiguration)
    expected_certs = 1u;
  EXPECT_EQ(expected_certs,
            trust_provider->GetAdditionalTrustAnchors().size());

  EXPECT_CALL(network_library_, RemoveNetworkProfileObserver(_));
}

TEST_P(NetworkConfigurationUpdaterTestWithParam, PolicyChange) {
  EXPECT_CALL(network_library_, AddNetworkProfileObserver(_));

  // Ignore the initial updates.
  EXPECT_CALL(network_library_, LoadOncNetworks(_, _))
      .Times(AnyNumber());
  StrictMock<chromeos::MockCertificateHandler>* certificate_handler =
      new StrictMock<chromeos::MockCertificateHandler>();
  EXPECT_CALL(*certificate_handler, ImportCertificates(_, _, _))
      .Times(AnyNumber());
  NetworkConfigurationUpdaterImplCros updater(
      policy_service_.get(),
      &network_library_,
      make_scoped_ptr<chromeos::CertificateHandler>(certificate_handler));
  updater.OnUserPolicyInitialized(false, "hash");
  Mock::VerifyAndClearExpectations(&network_library_);
  Mock::VerifyAndClearExpectations(&certificate_handler);

  // We should update if policy changes.
  EXPECT_CALL(network_library_, LoadOncNetworks(
      IsEqualTo(fake_network_configs_.get()), NameToONCSource(GetParam())));
  EXPECT_CALL(*certificate_handler, ImportCertificates(
      IsEqualTo(fake_certificates_.get()), NameToONCSource(GetParam()), _));

  // In the current implementation, we always apply both policies.
  EXPECT_CALL(network_library_, LoadOncNetworks(
      IsEqualTo(empty_network_configs_.get()),
      Ne(NameToONCSource(GetParam()))));
  EXPECT_CALL(*certificate_handler, ImportCertificates(
      IsEqualTo(empty_certificates_.get()),
      Ne(NameToONCSource(GetParam())),
      _));

  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             Value::CreateStringValue(kFakeONC));
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&network_library_);
  Mock::VerifyAndClearExpectations(&certificate_handler);

  // Another update is expected if the policy goes away. In the current
  // implementation, we always apply both policies.
  EXPECT_CALL(network_library_, LoadOncNetworks(
      IsEqualTo(empty_network_configs_.get()),
      onc::ONC_SOURCE_DEVICE_POLICY));
  EXPECT_CALL(*certificate_handler, ImportCertificates(
      IsEqualTo(empty_certificates_.get()),
      onc::ONC_SOURCE_DEVICE_POLICY,
      _));

  EXPECT_CALL(network_library_, LoadOncNetworks(
      IsEqualTo(empty_network_configs_.get()),
      onc::ONC_SOURCE_USER_POLICY));
  EXPECT_CALL(*certificate_handler, ImportCertificates(
      IsEqualTo(empty_certificates_.get()),
      onc::ONC_SOURCE_USER_POLICY,
      _));

  EXPECT_CALL(network_library_, RemoveNetworkProfileObserver(_));

  policy.Erase(GetParam());
  UpdateProviderPolicy(policy);
}

INSTANTIATE_TEST_CASE_P(
    NetworkConfigurationUpdaterTestWithParamInstance,
    NetworkConfigurationUpdaterTestWithParam,
    testing::Values(key::kDeviceOpenNetworkConfiguration,
                    key::kOpenNetworkConfiguration));

}  // namespace policy
