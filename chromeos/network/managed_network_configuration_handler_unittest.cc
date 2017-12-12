// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_shill_profile_client.h"
#include "chromeos/dbus/fake_shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace test_utils = ::chromeos::onc::test_utils;

namespace chromeos {

namespace {

constexpr char kUser1[] = "user1";
constexpr char kUser1ProfilePath[] = "/profile/user1/shill";

class TestNetworkProfileHandler : public NetworkProfileHandler {
 public:
  TestNetworkProfileHandler() {
    Init();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNetworkProfileHandler);
};

class TestNetworkPolicyObserver : public NetworkPolicyObserver {
 public:
  TestNetworkPolicyObserver() = default;

  void PoliciesApplied(const std::string& userhash) override {
    policies_applied_count_++;
  };

  int GetPoliciesAppliedCountAndReset() {
    int count = policies_applied_count_;
    policies_applied_count_ = 0;
    return count;
  }

 private:
  int policies_applied_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkPolicyObserver);
};

}  // namespace

class ManagedNetworkConfigurationHandlerTest : public testing::Test {
 public:
  ManagedNetworkConfigurationHandlerTest() {
    DBusThreadManager::Initialize();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_profile_handler_ = std::make_unique<TestNetworkProfileHandler>();
    network_configuration_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get(),
            nullptr /* no NetworkDeviceHandler */));

    // ManagedNetworkConfigurationHandlerImpl's ctor is private.
    managed_network_configuration_handler_.reset(
        new ManagedNetworkConfigurationHandlerImpl());
    managed_network_configuration_handler_->Init(
        network_state_handler_.get(), network_profile_handler_.get(),
        network_configuration_handler_.get(), nullptr /* no DeviceHandler */,
        nullptr /* no ProhibitedTechnologiesHandler */);
    managed_network_configuration_handler_->AddObserver(&policy_observer_);

    base::RunLoop().RunUntilIdle();
  }

  ~ManagedNetworkConfigurationHandlerTest() override {
    network_state_handler_->Shutdown();
    ResetManagedNetworkConfigurationHandler();
    network_configuration_handler_.reset();
    network_profile_handler_.reset();
    network_state_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  TestNetworkPolicyObserver* policy_observer() { return &policy_observer_; }

  ManagedNetworkConfigurationHandler* managed_handler() {
    return managed_network_configuration_handler_.get();
  }

  FakeShillServiceClient* GetShillServiceClient() {
    return static_cast<FakeShillServiceClient*>(
        DBusThreadManager::Get()->GetShillServiceClient());
  }

  FakeShillProfileClient* GetShillProfileClient() {
    return static_cast<FakeShillProfileClient*>(
        DBusThreadManager::Get()->GetShillProfileClient());
  }

  void InitializeStandardProfiles() {
    GetShillProfileClient()->AddProfile(kUser1ProfilePath, kUser1);
    GetShillProfileClient()->AddProfile(
        NetworkProfileHandler::GetSharedProfilePath(),
        std::string() /* no userhash */);
  }

  void SetPolicy(::onc::ONCSource onc_source,
                 const std::string& userhash,
                 const std::string& path_to_onc) {
    std::unique_ptr<base::DictionaryValue> policy =
        path_to_onc.empty()
            ? onc::ReadDictionaryFromJson(onc::kEmptyUnencryptedConfiguration)
            : test_utils::ReadTestDictionary(path_to_onc);

    base::ListValue network_configs;
    {
      const base::Value* found =
          policy->FindKeyOfType(::onc::toplevel_config::kNetworkConfigurations,
                                base::Value::Type::LIST);
      if (found)
        network_configs = base::ListValue(found->GetList());
    }

    base::DictionaryValue global_config;
    {
      const base::Value* found = policy->FindKeyOfType(
          ::onc::toplevel_config::kGlobalNetworkConfiguration,
          base::Value::Type::DICTIONARY);
      if (found) {
        global_config = std::move(*base::DictionaryValue::From(
            base::Value::ToUniquePtrValue(found->Clone())));
      }
    }

    managed_network_configuration_handler_->SetPolicy(
        onc_source, userhash, network_configs, global_config);
  }

  void SetUpEntry(const std::string& path_to_shill_json,
                  const std::string& profile_path,
                  const std::string& entry_path) {
    std::unique_ptr<base::DictionaryValue> entry =
        test_utils::ReadTestDictionary(path_to_shill_json);
    GetShillProfileClient()->AddEntry(profile_path, entry_path, *entry);
  }

  void ResetManagedNetworkConfigurationHandler() {
    if (!managed_network_configuration_handler_)
      return;
    managed_network_configuration_handler_->RemoveObserver(&policy_observer_);
    managed_network_configuration_handler_.reset();
  }

 private:
  base::MessageLoop message_loop_;

  TestNetworkPolicyObserver policy_observer_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<TestNetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_network_configuration_handler_;

  DISALLOW_COPY_AND_ASSIGN(ManagedNetworkConfigurationHandlerTest);
};

TEST_F(ManagedNetworkConfigurationHandlerTest, RemoveIrrelevantFields) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY,
            kUser1,
            "policy/policy_wifi1_with_redundant_fields.onc");
  base::RunLoop().RunUntilIdle();

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties("policy_wifi1");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnconfigured) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties("policy_wifi1");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, EnableManagedCredentialsWiFi) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_autoconnect_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_wifi1_autoconnect.onc");
  base::RunLoop().RunUntilIdle();

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties("policy_wifi1");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, EnableManagedCredentialsVPN) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_autoconnect_on_unconfigured_vpn.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_vpn_autoconnect.onc");
  base::RunLoop().RunUntilIdle();

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(
          "{a3860e83-f03d-4cb1-bafa-b22c9e746950}");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

// Ensure that EAP settings for ethernet are matched with the right profile
// entry and written to the dedicated EthernetEAP service.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyManageUnmanagedEthernetEAP) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/"
          "shill_policy_on_unmanaged_ethernet_eap.json");

  GetShillServiceClient()->AddService(
      "eth_entry", std::string() /* guid */, std::string() /* name */,
      "etherneteap", std::string() /* state */, true /* visible */);
  GetShillProfileClient()->AddService(kUser1ProfilePath, "eth_entry");
  SetUpEntry("policy/shill_unmanaged_ethernet_eap.json",
             kUser1ProfilePath,
             "eth_entry");

  // Also setup an unrelated WiFi configuration to verify that the right entry
  // is matched.
  GetShillServiceClient()->AddService(
      "wifi_entry", std::string() /* guid */, "wifi1", shill::kTypeWifi,
      std::string() /* state */, true /* visible */);
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "wifi_entry");

  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_ethernet_eap.onc");
  base::RunLoop().RunUntilIdle();

  // Verify eth_entry is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("eth_entry"));

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties("guid");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmodified) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "some_entry_path");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, PolicyApplicationRunning) {
  InitializeStandardProfiles();

  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  managed_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      std::string(),             // no userhash
      base::ListValue(),         // no device network policy
      base::DictionaryValue());  // no device global config

  EXPECT_TRUE(managed_handler()->IsAnyPolicyApplicationRunning());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "some_entry_path");

  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1_update.onc");
  EXPECT_TRUE(managed_handler()->IsAnyPolicyApplicationRunning());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, UpdatePolicyAfterFinished) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "some_entry_path");

  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1_update.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, UpdatePolicyBeforeFinished) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  // Usually the first call will cause a profile entry to be created, which we
  // don't fake here.
  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1_update.onc");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties("policy_wifi1");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedNewGUID) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties->RemoveWithoutPathExpansion(
      shill::kPassphraseProperty, NULL);

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties("policy_wifi1");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedVPN) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary("policy/shill_policy_on_managed_vpn.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_vpn.onc");
  base::RunLoop().RunUntilIdle();

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties(
          "{a3860e83-f03d-4cb1-bafa-b22c9e746950}");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyReapplyToManaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties->RemoveWithoutPathExpansion(
      shill::kPassphraseProperty, NULL);

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  {
    const base::DictionaryValue* properties =
        GetShillServiceClient()->GetServiceProperties("policy_wifi1");
    ASSERT_TRUE(properties);
    EXPECT_EQ(*expected_shill_properties, *properties);
  }

  // If we apply the policy again, without change, then the Shill profile will
  // not be modified.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  {
    const base::DictionaryValue* properties =
        GetShillServiceClient()->GetServiceProperties("policy_wifi1");
    ASSERT_TRUE(properties);
    EXPECT_EQ(*expected_shill_properties, *properties);
  }
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUnmanageManaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            std::string() /* path_to_onc */);
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetEmptyPolicyIgnoreUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            std::string() /* path_to_onc */);
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is kept.
  EXPECT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi2.json",
             kUser1ProfilePath,
             "wifi2_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties("policy_wifi1");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AutoConnectDisallowed) {
  InitializeStandardProfiles();

  // Setup an unmanaged network.
  SetUpEntry("policy/shill_unmanaged_wifi2.json",
             kUser1ProfilePath,
             "wifi2_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_disallow_autoconnect_on_unmanaged_wifi2.json");

  // Apply the user policy with global autoconnect config and expect that
  // autoconnect is disabled in the network's profile entry.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY,
            kUser1,
            "policy/policy_disallow_autoconnect.onc");
  base::RunLoop().RunUntilIdle();

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties("wifi2");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);

  // Verify that GetManagedProperties correctly augments the properties with the
  // global config from the user policy.
  // GetManagedProperties requires the device policy to be set or explicitly
  // unset.
  managed_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      std::string(),             // no userhash
      base::ListValue(),         // no device network policy
      base::DictionaryValue());  // no device global config

  std::unique_ptr<base::DictionaryValue> dictionary;
  managed_handler()->GetManagedProperties(
      kUser1, "wifi2",
      base::Bind(
          [](std::unique_ptr<base::DictionaryValue>* dictionary_out,
             const std::string& service_path,
             const base::DictionaryValue& dictionary) {
            *dictionary_out = base::DictionaryValue::From(
                base::Value::ToUniquePtrValue(dictionary.Clone()));
          },
          &dictionary),
      base::Bind(
          [](const std::string& error_name,
             std::unique_ptr<base::DictionaryValue> error_data) { FAIL(); }));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(dictionary.get());
  std::unique_ptr<base::DictionaryValue> expected_managed_onc =
      test_utils::ReadTestDictionary(
          "policy/"
          "managed_onc_disallow_autoconnect_on_unmanaged_wifi2.onc");
  EXPECT_EQ(*expected_managed_onc, *dictionary);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, LateProfileLoading) {
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  InitializeStandardProfiles();
  base::RunLoop().RunUntilIdle();

  const base::DictionaryValue* properties =
      GetShillServiceClient()->GetServiceProperties("policy_wifi1");
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       ShutdownDuringPolicyApplication) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");

  // Reset the network configuration manager after setting policy and before
  // calling RunUntilIdle to simulate shutdown during policy application.
  ResetManagedNetworkConfigurationHandler();
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
