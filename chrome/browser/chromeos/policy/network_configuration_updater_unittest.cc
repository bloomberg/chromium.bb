// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/network/onc/onc_constants.h"
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
using testing::_;

namespace policy {

namespace {

const char kFakeONC[] = "{ \"GUID\": \"1234\" }";

ACTION_P(SetCertificateList, list) {
  *arg3 = list;
  return true;
}

}  // namespace

class NetworkConfigurationUpdaterTest
    : public testing::TestWithParam<const char*>{
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

    CommandLine* command_line = CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kEnableWebTrustCerts);
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
  static chromeos::onc::ONCSource NameToONCSource(
      const std::string& name) {
    if (name == key::kDeviceOpenNetworkConfiguration)
      return chromeos::onc::ONC_SOURCE_DEVICE_POLICY;
    if (name == key::kOpenNetworkConfiguration)
      return chromeos::onc::ONC_SOURCE_USER_POLICY;
    return chromeos::onc::ONC_SOURCE_NONE;
  }

  chromeos::MockNetworkLibrary network_library_;
  MockConfigurationPolicyProvider provider_;
  scoped_ptr<PolicyServiceImpl> policy_service_;
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

TEST_P(NetworkConfigurationUpdaterTest, InitialUpdates) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             Value::CreateStringValue(kFakeONC));
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_library_, AddNetworkProfileObserver(_));

  // Initially, only the device policy is applied. The user policy is only
  // applied after the user profile was initialized.
  const char* device_onc = GetParam() == key::kDeviceOpenNetworkConfiguration ?
      kFakeONC : chromeos::onc::kEmptyUnencryptedConfiguration;
  EXPECT_CALL(network_library_, LoadOncNetworks(
      device_onc, "", chromeos::onc::ONC_SOURCE_DEVICE_POLICY, _));

  {
    NetworkConfigurationUpdater updater(policy_service_.get(),
                                        &network_library_);
    Mock::VerifyAndClearExpectations(&network_library_);

    // After the user policy is initialized, we always push both policies to the
    // NetworkLibrary.
    EXPECT_CALL(network_library_, LoadOncNetworks(
        kFakeONC, "", NameToONCSource(GetParam()), _));
    EXPECT_CALL(network_library_, LoadOncNetworks(
        chromeos::onc::kEmptyUnencryptedConfiguration,
        "",
        Ne(NameToONCSource(GetParam())),
        _));

    EXPECT_CALL(network_library_, RemoveNetworkProfileObserver(_));

    updater.OnUserPolicyInitialized();
  }
  Mock::VerifyAndClearExpectations(&network_library_);
}

TEST_P(NetworkConfigurationUpdaterTest, AllowTrustedCertificatesFromPolicy) {
  {
    EXPECT_CALL(network_library_, AddNetworkProfileObserver(_));

    const net::CertificateList empty_cert_list;

    const net::CertificateList cert_list =
        net::CreateCertificateListFromFile(net::GetTestCertsDirectory(),
                                           "ok_cert.pem",
                                           net::X509Certificate::FORMAT_AUTO);
    ASSERT_EQ(1u, cert_list.size());

    EXPECT_CALL(network_library_, LoadOncNetworks(_, _, _, _))
        .WillRepeatedly(SetCertificateList(empty_cert_list));
    NetworkConfigurationUpdater updater(policy_service_.get(),
                                        &network_library_);
    net::CertTrustAnchorProvider* trust_provider =
        updater.GetCertTrustAnchorProvider();
    ASSERT_TRUE(trust_provider);
    // The initial list of trust anchors is empty.
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
    EXPECT_TRUE(trust_provider->GetAdditionalTrustAnchors().empty());

    // Initially, certificates imported from policy don't have trust flags.
    updater.OnUserPolicyInitialized();
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
    Mock::VerifyAndClearExpectations(&network_library_);
    EXPECT_TRUE(trust_provider->GetAdditionalTrustAnchors().empty());

    // Certificates with the "Web" trust flag set should be forwarded to the
    // trust provider.
    EXPECT_CALL(network_library_, LoadOncNetworks(_, _, _, _))
        .WillRepeatedly(SetCertificateList(empty_cert_list));
    chromeos::onc::ONCSource current_source = NameToONCSource(GetParam());
    EXPECT_CALL(network_library_, LoadOncNetworks(_, _, current_source, _))
        .WillRepeatedly(SetCertificateList(cert_list));
    updater.set_allow_trusted_certificates_from_policy(true);
    // Trigger a policy update.
    PolicyMap policy;
    policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateStringValue(kFakeONC));
    UpdateProviderPolicy(policy);
    Mock::VerifyAndClearExpectations(&network_library_);

    // Certificates are only provided as trust anchors if they come from user
    // policy.
    size_t expected_certs = 0u;
    if (GetParam() == key::kOpenNetworkConfiguration)
      expected_certs = 1u;
    EXPECT_EQ(expected_certs,
              trust_provider->GetAdditionalTrustAnchors().size());

    EXPECT_CALL(network_library_, RemoveNetworkProfileObserver(_));
  }
  Mock::VerifyAndClearExpectations(&network_library_);
}

TEST_P(NetworkConfigurationUpdaterTest, PolicyChange) {
  {
    EXPECT_CALL(network_library_, AddNetworkProfileObserver(_));

    // Ignore the initial updates.
    EXPECT_CALL(network_library_, LoadOncNetworks(_, _, _, _))
        .Times(AnyNumber());
    NetworkConfigurationUpdater updater(policy_service_.get(),
                                        &network_library_);
    updater.OnUserPolicyInitialized();
    Mock::VerifyAndClearExpectations(&network_library_);

    // We should update if policy changes.
    EXPECT_CALL(network_library_, LoadOncNetworks(
        kFakeONC, "", NameToONCSource(GetParam()), _));

    // In the current implementation, we always apply both policies.
    EXPECT_CALL(network_library_, LoadOncNetworks(
        chromeos::onc::kEmptyUnencryptedConfiguration,
        "",
        Ne(NameToONCSource(GetParam())),
        _));

    PolicyMap policy;
    policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               Value::CreateStringValue(kFakeONC));
    UpdateProviderPolicy(policy);
    Mock::VerifyAndClearExpectations(&network_library_);

    // Another update is expected if the policy goes away. In the current
    // implementation, we always apply both policies.
    EXPECT_CALL(network_library_, LoadOncNetworks(
        chromeos::onc::kEmptyUnencryptedConfiguration, "",
        chromeos::onc::ONC_SOURCE_DEVICE_POLICY, _));

    EXPECT_CALL(network_library_, LoadOncNetworks(
        chromeos::onc::kEmptyUnencryptedConfiguration, "",
        chromeos::onc::ONC_SOURCE_USER_POLICY, _));

    EXPECT_CALL(network_library_, RemoveNetworkProfileObserver(_));

    policy.Erase(GetParam());
    UpdateProviderPolicy(policy);
  }
  Mock::VerifyAndClearExpectations(&network_library_);
}

INSTANTIATE_TEST_CASE_P(
    NetworkConfigurationUpdaterTestInstance,
    NetworkConfigurationUpdaterTest,
    testing::Values(key::kDeviceOpenNetworkConfiguration,
                    key::kOpenNetworkConfiguration));

}  // namespace policy
