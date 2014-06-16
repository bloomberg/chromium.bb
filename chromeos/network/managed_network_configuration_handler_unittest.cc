// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/dbus/mock_shill_profile_client.h"
#include "chromeos/dbus/shill_client_helper.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::_;

namespace test_utils = ::chromeos::onc::test_utils;

namespace chromeos {

namespace {

std::string ValueToString(const base::Value* value) {
  std::stringstream str;
  str << *value;
  return str.str();
}

const char kUser1[] = "user1";
const char kUser1ProfilePath[] = "/profile/user1/shill";

// Matcher to match base::Value.
MATCHER_P(IsEqualTo,
          value,
          std::string(negation ? "isn't" : "is") + " equal to " +
          ValueToString(value)) {
  return value->Equals(&arg);
}

class ShillProfileTestClient {
 public:
  typedef ShillClientHelper::DictionaryValueCallbackWithoutStatus
      DictionaryValueCallbackWithoutStatus;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  void AddProfile(const std::string& profile_path,
                  const std::string& userhash) {
    if (profile_entries_.HasKey(profile_path))
      return;

    base::DictionaryValue* profile = new base::DictionaryValue;
    profile_entries_.SetWithoutPathExpansion(profile_path, profile);
    profile_to_user_[profile_path] = userhash;
  }

  void AddEntry(const std::string& profile_path,
                const std::string& entry_path,
                const base::DictionaryValue& entry) {
    base::DictionaryValue* entries = NULL;
    profile_entries_.GetDictionaryWithoutPathExpansion(profile_path, &entries);
    ASSERT_TRUE(entries);

    base::DictionaryValue* new_entry = entry.DeepCopy();
    new_entry->SetStringWithoutPathExpansion(shill::kProfileProperty,
                                             profile_path);
    entries->SetWithoutPathExpansion(entry_path, new_entry);
  }

  void GetProperties(const dbus::ObjectPath& profile_path,
                     const DictionaryValueCallbackWithoutStatus& callback,
                     const ErrorCallback& error_callback) {
    base::DictionaryValue* entries = NULL;
    profile_entries_.GetDictionaryWithoutPathExpansion(profile_path.value(),
                                                       &entries);
    ASSERT_TRUE(entries);

    scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue);
    base::ListValue* entry_paths = new base::ListValue;
    result->SetWithoutPathExpansion(shill::kEntriesProperty, entry_paths);
    for (base::DictionaryValue::Iterator it(*entries); !it.IsAtEnd();
         it.Advance()) {
      entry_paths->AppendString(it.key());
    }

    ASSERT_TRUE(ContainsKey(profile_to_user_, profile_path.value()));
    const std::string& userhash = profile_to_user_[profile_path.value()];
    result->SetStringWithoutPathExpansion(shill::kUserHashProperty, userhash);

    callback.Run(*result);
  }

  void GetEntry(const dbus::ObjectPath& profile_path,
                const std::string& entry_path,
                const DictionaryValueCallbackWithoutStatus& callback,
                const ErrorCallback& error_callback) {
    base::DictionaryValue* entries = NULL;
    profile_entries_.GetDictionaryWithoutPathExpansion(profile_path.value(),
                                                       &entries);
    ASSERT_TRUE(entries);

    base::DictionaryValue* entry = NULL;
    entries->GetDictionaryWithoutPathExpansion(entry_path, &entry);
    ASSERT_TRUE(entry);
    callback.Run(*entry);
  }

 protected:
  base::DictionaryValue profile_entries_;
  std::map<std::string, std::string> profile_to_user_;
};

class TestNetworkProfileHandler : public NetworkProfileHandler {
 public:
  TestNetworkProfileHandler() {
    Init();
  }
  virtual ~TestNetworkProfileHandler() {}

  void AddProfileForTest(const NetworkProfile& profile) {
    AddProfile(profile);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNetworkProfileHandler);
};

}  // namespace

class ManagedNetworkConfigurationHandlerTest : public testing::Test {
 public:
  ManagedNetworkConfigurationHandlerTest()
      : mock_manager_client_(NULL),
        mock_profile_client_(NULL) {
  }

  virtual ~ManagedNetworkConfigurationHandlerTest() {
  }

  virtual void SetUp() OVERRIDE {
    FakeDBusThreadManager* dbus_thread_manager = new FakeDBusThreadManager;
    mock_manager_client_ = new StrictMock<MockShillManagerClient>();
    mock_profile_client_ = new StrictMock<MockShillProfileClient>();
    dbus_thread_manager->SetShillManagerClient(
        scoped_ptr<ShillManagerClient>(mock_manager_client_).Pass());
    dbus_thread_manager->SetShillProfileClient(
        scoped_ptr<ShillProfileClient>(mock_profile_client_).Pass());

    DBusThreadManager::InitializeForTesting(dbus_thread_manager);

    SetNetworkConfigurationHandlerExpectations();

    ON_CALL(*mock_profile_client_, GetProperties(_,_,_))
        .WillByDefault(Invoke(&profiles_stub_,
                              &ShillProfileTestClient::GetProperties));

    ON_CALL(*mock_profile_client_, GetEntry(_,_,_,_))
        .WillByDefault(Invoke(&profiles_stub_,
                              &ShillProfileTestClient::GetEntry));

    network_profile_handler_.reset(new TestNetworkProfileHandler());
    network_configuration_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            NULL /* no NetworkStateHandler */));
    managed_network_configuration_handler_.reset(
        new ManagedNetworkConfigurationHandlerImpl());
    managed_network_configuration_handler_->Init(
        NULL /* no NetworkStateHandler */,
        network_profile_handler_.get(),
        network_configuration_handler_.get(),
        NULL /* no DeviceHandler */);

    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    managed_network_configuration_handler_.reset();
    network_configuration_handler_.reset();
    network_profile_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  void VerifyAndClearExpectations() {
    Mock::VerifyAndClearExpectations(mock_manager_client_);
    Mock::VerifyAndClearExpectations(mock_profile_client_);
    SetNetworkConfigurationHandlerExpectations();
  }

  void InitializeStandardProfiles() {
    profiles_stub_.AddProfile(kUser1ProfilePath, kUser1);
    network_profile_handler_->
        AddProfileForTest(NetworkProfile(kUser1ProfilePath, kUser1));
    network_profile_handler_->
        AddProfileForTest(NetworkProfile(
            NetworkProfileHandler::GetSharedProfilePath(), std::string()));
  }

  void SetUpEntry(const std::string& path_to_shill_json,
                  const std::string& profile_path,
                  const std::string& entry_path) {
    scoped_ptr<base::DictionaryValue> entry =
        test_utils::ReadTestDictionary(path_to_shill_json);
    profiles_stub_.AddEntry(profile_path, entry_path, *entry);
  }

  void SetPolicy(::onc::ONCSource onc_source,
                 const std::string& userhash,
                 const std::string& path_to_onc) {
    scoped_ptr<base::DictionaryValue> policy;
    if (path_to_onc.empty())
      policy = onc::ReadDictionaryFromJson(onc::kEmptyUnencryptedConfiguration);
    else
      policy = test_utils::ReadTestDictionary(path_to_onc);

    base::ListValue empty_network_configs;
    base::ListValue* network_configs = &empty_network_configs;
    policy->GetListWithoutPathExpansion(
        ::onc::toplevel_config::kNetworkConfigurations, &network_configs);

    base::DictionaryValue empty_global_config;
    base::DictionaryValue* global_network_config = &empty_global_config;
    policy->GetDictionaryWithoutPathExpansion(
        ::onc::toplevel_config::kGlobalNetworkConfiguration,
        &global_network_config);

    managed_handler()->SetPolicy(
        onc_source, userhash, *network_configs, *global_network_config);
  }

  void SetNetworkConfigurationHandlerExpectations() {
    // These calls occur in NetworkConfigurationHandler.
    EXPECT_CALL(*mock_manager_client_, GetProperties(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_manager_client_,
                AddPropertyChangedObserver(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_manager_client_,
                RemovePropertyChangedObserver(_)).Times(AnyNumber());
  }

  ManagedNetworkConfigurationHandler* managed_handler() {
    return managed_network_configuration_handler_.get();
  }

 protected:
  MockShillManagerClient* mock_manager_client_;
  MockShillProfileClient* mock_profile_client_;
  ShillProfileTestClient profiles_stub_;
  scoped_ptr<TestNetworkProfileHandler> network_profile_handler_;
  scoped_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  scoped_ptr<ManagedNetworkConfigurationHandlerImpl>
        managed_network_configuration_handler_;
  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedNetworkConfigurationHandlerTest);
};

TEST_F(ManagedNetworkConfigurationHandlerTest, ProfileInitialization) {
  InitializeStandardProfiles();
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, RemoveIrrelevantFields) {
  InitializeStandardProfiles();
  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY,
            kUser1,
            "policy/policy_wifi1_with_redundant_fields.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnconfigured) {
  InitializeStandardProfiles();
  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

// Ensure that EAP settings for ethernet are matched with the right profile
// entry and written to the dedicated EthernetEAP service.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyManageUnmanagedEthernetEAP) {
  InitializeStandardProfiles();
  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/"
          "shill_policy_on_unmanaged_ethernet_eap.json");

  SetUpEntry("policy/shill_unmanaged_ethernet_eap.json",
             kUser1ProfilePath,
             "eth_entry");

  // Also setup an unrelated WiFi configuration to verify that the right entry
  // is matched.
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "wifi_entry");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(*mock_profile_client_,
              GetEntry(dbus::ObjectPath(kUser1ProfilePath), _, _, _)).Times(2);

  EXPECT_CALL(
      *mock_profile_client_,
      DeleteEntry(dbus::ObjectPath(kUser1ProfilePath), "eth_entry", _, _));

  EXPECT_CALL(
      *mock_manager_client_,
      ConfigureServiceForProfile(dbus::ObjectPath(kUser1ProfilePath),
                                 IsEqualTo(expected_shill_properties.get()),
                                 _, _));

  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_ethernet_eap.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmodified) {
  InitializeStandardProfiles();
  EXPECT_CALL(*mock_profile_client_, GetProperties(_, _, _));

  EXPECT_CALL(*mock_manager_client_, ConfigureServiceForProfile(_, _, _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
  VerifyAndClearExpectations();

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "some_entry_path");

  EXPECT_CALL(*mock_profile_client_, GetProperties(_, _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "some_entry_path", _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "old_entry_path", _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      DeleteEntry(dbus::ObjectPath(kUser1ProfilePath), "old_entry_path", _, _));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

// Old ChromeOS versions may not have used the UIData property
TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyManageUnmanagedWithoutUIData) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "old_entry_path", _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      DeleteEntry(dbus::ObjectPath(kUser1ProfilePath), "old_entry_path", _, _));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedNewGUID) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties->RemoveWithoutPathExpansion(
      shill::kPassphraseProperty, NULL);

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "old_entry_path", _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      DeleteEntry(dbus::ObjectPath(kUser1ProfilePath), "old_entry_path", _, _));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedVPN) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_managed_vpn.json");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "entry_path", _, _));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_vpn.onc");
  message_loop_.RunUntilIdle();
  VerifyAndClearExpectations();
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyUpdateManagedEquivalentSecurity) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_wifi1_rsn.json",
             kUser1ProfilePath,
             "old_entry_path");

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties->RemoveWithoutPathExpansion(
      shill::kPassphraseProperty, NULL);

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "old_entry_path", _, _));

  // The existing entry must not be deleted because the Security type 'rsa' is
  // equivalent to 'psk' when identifying networks.

  EXPECT_CALL(
      *mock_manager_client_,
      ConfigureServiceForProfile(dbus::ObjectPath(kUser1ProfilePath),
                                 IsEqualTo(expected_shill_properties.get()),
                                 _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyReapplyToManaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties->RemoveWithoutPathExpansion(
      shill::kPassphraseProperty, NULL);

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "old_entry_path", _, _));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
  VerifyAndClearExpectations();

  // If we apply the policy again, without change, then the Shill profile will
  // not be modified.
  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "old_entry_path", _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUnmanageManaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(*mock_profile_client_,
              GetEntry(dbus::ObjectPath(kUser1ProfilePath),
                       "old_entry_path",
                       _, _));

  EXPECT_CALL(*mock_profile_client_,
              DeleteEntry(dbus::ObjectPath(kUser1ProfilePath),
                          "old_entry_path",
                          _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetEmptyPolicyIgnoreUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(*mock_profile_client_,
              GetEntry(dbus::ObjectPath(kUser1ProfilePath),
                       "old_entry_path",
                       _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi2.json",
             kUser1ProfilePath,
             "wifi2_entry_path");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "wifi2_entry_path", _, _));

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AutoConnectDisallowed) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi2.json",
             kUser1ProfilePath,
             "wifi2_entry_path");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "wifi2_entry_path", _, _));

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_disallow_autoconnect_on_unmanaged_wifi2.json");

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY,
            kUser1,
            "policy/policy_disallow_autoconnect.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, LateProfileLoading) {
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");

  message_loop_.RunUntilIdle();
  VerifyAndClearExpectations();

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  InitializeStandardProfiles();
  message_loop_.RunUntilIdle();
}

class ManagedNetworkConfigurationHandlerShutdownTest
    : public ManagedNetworkConfigurationHandlerTest {
 public:
  virtual void SetUp() OVERRIDE {
    ManagedNetworkConfigurationHandlerTest::SetUp();
    ON_CALL(*mock_profile_client_, GetProperties(_, _, _)).WillByDefault(
        Invoke(&ManagedNetworkConfigurationHandlerShutdownTest::GetProperties));
  }

  static void GetProperties(
      const dbus::ObjectPath& profile_path,
      const ShillClientHelper::DictionaryValueCallbackWithoutStatus& callback,
      const ShillClientHelper::ErrorCallback& error_callback) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(ManagedNetworkConfigurationHandlerShutdownTest::
                       CallbackWithEmptyDictionary,
                   callback));
  }

  static void CallbackWithEmptyDictionary(
      const ShillClientHelper::DictionaryValueCallbackWithoutStatus& callback) {
    callback.Run(base::DictionaryValue());
  }
};

TEST_F(ManagedNetworkConfigurationHandlerShutdownTest,
       DuringPolicyApplication) {
  InitializeStandardProfiles();

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  managed_network_configuration_handler_.reset();
  message_loop_.RunUntilIdle();
}

}  // namespace chromeos
