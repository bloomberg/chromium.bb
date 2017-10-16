// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <sstream>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/dbus/mock_shill_profile_client.h"
#include "chromeos/dbus/mock_shill_service_client.h"
#include "chromeos/dbus/shill_client_helper.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
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

void DereferenceAndCall(
    base::Callback<void(const base::DictionaryValue& result)> callback,
    const base::DictionaryValue* value) {
  callback.Run(*value);
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

// Match properties in |value| to |arg|. |arg| may contain extra properties).
MATCHER_P(MatchesProperties,
          value,
          std::string(negation ? "does't match " : "matches ") +
              ValueToString(value)) {
  for (base::DictionaryValue::Iterator iter(*value); !iter.IsAtEnd();
       iter.Advance()) {
    const base::Value* property;
    if (!arg.GetWithoutPathExpansion(iter.key(), &property) ||
        !iter.value().Equals(property)) {
      return false;
    }
  }
  return true;
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

    profile_entries_.SetWithoutPathExpansion(
        profile_path, std::make_unique<base::DictionaryValue>());
    profile_to_user_[profile_path] = userhash;
  }

  void AddEntry(const std::string& profile_path,
                const std::string& entry_path,
                const base::DictionaryValue& entry) {
    base::DictionaryValue* entries = NULL;
    profile_entries_.GetDictionaryWithoutPathExpansion(profile_path, &entries);
    ASSERT_TRUE(entries);

    base::Value new_entry = entry.Clone();
    new_entry.SetKey(shill::kProfileProperty, base::Value(profile_path));
    entries->SetKey(entry_path, std::move(new_entry));
  }

  void GetProperties(const dbus::ObjectPath& profile_path,
                     const DictionaryValueCallbackWithoutStatus& callback,
                     const ErrorCallback& error_callback) {
    base::DictionaryValue* entries = NULL;
    profile_entries_.GetDictionaryWithoutPathExpansion(profile_path.value(),
                                                       &entries);
    ASSERT_TRUE(entries);

    std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
    auto entry_paths = std::make_unique<base::ListValue>();
    for (base::DictionaryValue::Iterator it(*entries); !it.IsAtEnd();
         it.Advance()) {
      entry_paths->AppendString(it.key());
    }
    result->SetWithoutPathExpansion(shill::kEntriesProperty,
                                    std::move(entry_paths));

    ASSERT_TRUE(base::ContainsKey(profile_to_user_, profile_path.value()));
    const std::string& userhash = profile_to_user_[profile_path.value()];
    result->SetKey(shill::kUserHashProperty, base::Value(userhash));

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&DereferenceAndCall, callback,
                              base::Owned(result.release())));
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&DereferenceAndCall, callback,
                              base::Owned(entry->DeepCopy())));
  }

 protected:
  base::DictionaryValue profile_entries_;
  std::map<std::string, std::string> profile_to_user_;
};

class ShillServiceTestClient {
 public:
  typedef ShillClientHelper::DictionaryValueCallback DictionaryValueCallback;
  void SetFakeProperties(const base::DictionaryValue& service_properties) {
    service_properties_.Clear();
    service_properties_.MergeDictionary(&service_properties);
  }

  void GetProperties(const dbus::ObjectPath& service_path,
                     const DictionaryValueCallback& callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, DBUS_METHOD_CALL_SUCCESS,
                              base::ConstRef(service_properties_)));
  }

 protected:
  base::DictionaryValue service_properties_;
};

class TestNetworkProfileHandler : public NetworkProfileHandler {
 public:
  TestNetworkProfileHandler() {
    Init();
  }
  ~TestNetworkProfileHandler() override {}

  void AddProfileForTest(const NetworkProfile& profile) {
    AddProfile(profile);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNetworkProfileHandler);
};

class TestNetworkPolicyObserver : public NetworkPolicyObserver {
 public:
  TestNetworkPolicyObserver() : policies_applied_count_(0) {}

  void PoliciesApplied(const std::string& userhash) override {
    policies_applied_count_++;
  };

  int GetPoliciesAppliedCountAndReset() {
    int count = policies_applied_count_;
    policies_applied_count_ = 0;
    return count;
  }

 private:
  int policies_applied_count_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkPolicyObserver);
};

}  // namespace

class ManagedNetworkConfigurationHandlerTest : public testing::Test {
 public:
  ManagedNetworkConfigurationHandlerTest()
      : mock_manager_client_(NULL),
        mock_profile_client_(NULL),
        mock_service_client_(NULL) {
  }

  ~ManagedNetworkConfigurationHandlerTest() override {}

  void SetUp() override {
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        DBusThreadManager::GetSetterForTesting();
    mock_manager_client_ = new StrictMock<MockShillManagerClient>();
    mock_profile_client_ = new StrictMock<MockShillProfileClient>();
    mock_service_client_ = new StrictMock<MockShillServiceClient>();
    dbus_setter->SetShillManagerClient(
        std::unique_ptr<ShillManagerClient>(mock_manager_client_));
    dbus_setter->SetShillProfileClient(
        std::unique_ptr<ShillProfileClient>(mock_profile_client_));
    dbus_setter->SetShillServiceClient(
        std::unique_ptr<ShillServiceClient>(mock_service_client_));

    SetNetworkConfigurationHandlerExpectations();

    ON_CALL(*mock_profile_client_, GetProperties(_,_,_))
        .WillByDefault(Invoke(&profiles_stub_,
                              &ShillProfileTestClient::GetProperties));

    ON_CALL(*mock_profile_client_, GetEntry(_,_,_,_))
        .WillByDefault(Invoke(&profiles_stub_,
                              &ShillProfileTestClient::GetEntry));

    ON_CALL(*mock_service_client_, GetProperties(_,_))
        .WillByDefault(Invoke(&services_stub_,
                              &ShillServiceTestClient::GetProperties));

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_profile_handler_.reset(new TestNetworkProfileHandler());
    network_configuration_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get(),
            nullptr /* no NetworkDeviceHandler */));
    managed_network_configuration_handler_.reset(
        new ManagedNetworkConfigurationHandlerImpl());
    managed_network_configuration_handler_->Init(
        network_state_handler_.get(), network_profile_handler_.get(),
        network_configuration_handler_.get(), nullptr /* no DeviceHandler */,
        nullptr /* no ProhibitedTechnologiesHandler */);
    managed_network_configuration_handler_->AddObserver(&policy_observer_);

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_handler_->Shutdown();
    if (managed_network_configuration_handler_)
      managed_network_configuration_handler_->RemoveObserver(&policy_observer_);
    managed_network_configuration_handler_.reset();
    network_configuration_handler_.reset();
    network_profile_handler_.reset();
    network_state_handler_.reset();
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

    profiles_stub_.AddProfile(NetworkProfileHandler::GetSharedProfilePath(),
                              std::string() /* no userhash */);
    network_profile_handler_->AddProfileForTest(
        NetworkProfile(NetworkProfileHandler::GetSharedProfilePath(),
                       std::string() /* no userhash */));
  }

  void SetUpEntry(const std::string& path_to_shill_json,
                  const std::string& profile_path,
                  const std::string& entry_path) {
    std::unique_ptr<base::DictionaryValue> entry =
        test_utils::ReadTestDictionary(path_to_shill_json);
    profiles_stub_.AddEntry(profile_path, entry_path, *entry);
  }

  void SetPolicy(::onc::ONCSource onc_source,
                 const std::string& userhash,
                 const std::string& path_to_onc) {
    std::unique_ptr<base::DictionaryValue> policy;
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
                SetProperty("ProhibitedTechnologies", _, _, _))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_manager_client_, AddPropertyChangedObserver(_))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_manager_client_,
                RemovePropertyChangedObserver(_)).Times(AnyNumber());
  }

  ManagedNetworkConfigurationHandler* managed_handler() {
    return managed_network_configuration_handler_.get();
  }

  void GetManagedProperties(const std::string& userhash,
                            const std::string& service_path) {
    managed_handler()->GetManagedProperties(
        userhash,
        service_path,
        base::Bind(
            &ManagedNetworkConfigurationHandlerTest::GetPropertiesCallback,
            base::Unretained(this)),
        base::Bind(&ManagedNetworkConfigurationHandlerTest::UnexpectedError));
  }

  void GetPropertiesCallback(const std::string& service_path,
                             const base::DictionaryValue& dictionary) {
    get_properties_service_path_ = service_path;
    get_properties_result_.Clear();
    get_properties_result_.MergeDictionary(&dictionary);
  }

  static void UnexpectedError(
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data) {
    ASSERT_FALSE(true);
  }

 protected:
  MockShillManagerClient* mock_manager_client_;
  MockShillProfileClient* mock_profile_client_;
  MockShillServiceClient* mock_service_client_;
  ShillProfileTestClient profiles_stub_;
  ShillServiceTestClient services_stub_;
  TestNetworkPolicyObserver policy_observer_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<TestNetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_network_configuration_handler_;
  base::MessageLoop message_loop_;

  std::string get_properties_service_path_;
  base::DictionaryValue get_properties_result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedNetworkConfigurationHandlerTest);
};

TEST_F(ManagedNetworkConfigurationHandlerTest, ProfileInitialization) {
  InitializeStandardProfiles();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, RemoveIrrelevantFields) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
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
  base::RunLoop().RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnconfigured) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
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
  base::RunLoop().RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, EnableManagedCredentialsWiFi) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_autoconnect_on_unconfigured_wifi1.json");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_wifi1_autoconnect.onc");
  base::RunLoop().RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, EnableManagedCredentialsVPN) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_autoconnect_on_unconfigured_vpn.json");

  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_vpn_autoconnect.onc");
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmodified) {
  InitializeStandardProfiles();
  EXPECT_CALL(*mock_profile_client_, GetProperties(_, _, _));

  EXPECT_CALL(*mock_manager_client_, ConfigureServiceForProfile(_, _, _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer_.GetPoliciesAppliedCountAndReset());
  VerifyAndClearExpectations();

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "some_entry_path");

  EXPECT_CALL(*mock_profile_client_, GetProperties(_, _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "some_entry_path", _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer_.GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, PolicyApplicationRunning) {
  InitializeStandardProfiles();
  EXPECT_CALL(*mock_profile_client_, GetProperties(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_manager_client_, ConfigureServiceForProfile(_, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_profile_client_, GetEntry(_, _, _, _)).Times(AnyNumber());

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
  EXPECT_CALL(*mock_profile_client_, GetProperties(_, _, _));
  EXPECT_CALL(*mock_manager_client_, ConfigureServiceForProfile(_, _, _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer_.GetPoliciesAppliedCountAndReset());
  VerifyAndClearExpectations();

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "some_entry_path");

  EXPECT_CALL(*mock_profile_client_, GetProperties(_, _, _));
  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "some_entry_path", _, _));
  EXPECT_CALL(*mock_manager_client_, ConfigureServiceForProfile(_, _, _, _));

  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1_update.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer_.GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, UpdatePolicyBeforeFinished) {
  InitializeStandardProfiles();
  EXPECT_CALL(*mock_profile_client_, GetProperties(_, _, _)).Times(2);
  EXPECT_CALL(*mock_manager_client_, ConfigureServiceForProfile(_, _, _, _))
      .Times(2);

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  // Usually the first call will cause a profile entry to be created, which we
  // don't fake here.
  SetPolicy(
      ::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1_update.onc");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer_.GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
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
  base::RunLoop().RunUntilIdle();
}

// Old ChromeOS versions may not have used the UIData property
TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyManageUnmanagedWithoutUIData) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json",
             kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
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
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedVPN) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary("policy/shill_policy_on_managed_vpn.json");

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
  base::RunLoop().RunUntilIdle();
  VerifyAndClearExpectations();
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
  base::RunLoop().RunUntilIdle();
  VerifyAndClearExpectations();

  // If we apply the policy again, without change, then the Shill profile will
  // not be modified.
  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "old_entry_path", _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer_.GetPoliciesAppliedCountAndReset());
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

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AutoConnectDisallowed) {
  InitializeStandardProfiles();
  // Setup an unmanaged network.
  SetUpEntry("policy/shill_unmanaged_wifi2.json",
             kUser1ProfilePath,
             "wifi2_entry_path");

  // Apply the user policy with global autoconnect config and expect that
  // autoconnect is disabled in the network's profile entry.
  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(kUser1ProfilePath), _, _));

  EXPECT_CALL(
      *mock_profile_client_,
      GetEntry(dbus::ObjectPath(kUser1ProfilePath), "wifi2_entry_path", _, _));

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_disallow_autoconnect_on_unmanaged_wifi2.json");

  EXPECT_CALL(*mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath(kUser1ProfilePath),
                  MatchesProperties(expected_shill_properties.get()), _, _));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY,
            kUser1,
            "policy/policy_disallow_autoconnect.onc");
  base::RunLoop().RunUntilIdle();

  // Verify that GetManagedProperties correctly augments the properties with the
  // global config from the user policy.

  // GetManagedProperties requires the device policy to be set or explicitly
  // unset.
  EXPECT_CALL(*mock_profile_client_,
              GetProperties(dbus::ObjectPath(
                                NetworkProfileHandler::GetSharedProfilePath()),
                            _,
                            _));
  managed_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      std::string(),             // no userhash
      base::ListValue(),         // no device network policy
      base::DictionaryValue());  // no device global config

  services_stub_.SetFakeProperties(*expected_shill_properties);
  EXPECT_CALL(*mock_service_client_,
              GetProperties(dbus::ObjectPath(
                                "wifi2"),_));

  GetManagedProperties(kUser1, "wifi2");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("wifi2", get_properties_service_path_);

  std::unique_ptr<base::DictionaryValue> expected_managed_onc =
      test_utils::ReadTestDictionary(
          "policy/managed_onc_disallow_autoconnect_on_unmanaged_wifi2.onc");
  EXPECT_TRUE(onc::test_utils::Equals(expected_managed_onc.get(),
                                      &get_properties_result_));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, LateProfileLoading) {
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");

  base::RunLoop().RunUntilIdle();
  VerifyAndClearExpectations();

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
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
  base::RunLoop().RunUntilIdle();
}

class ManagedNetworkConfigurationHandlerShutdownTest
    : public ManagedNetworkConfigurationHandlerTest {
 public:
  void SetUp() override {
    ManagedNetworkConfigurationHandlerTest::SetUp();
    ON_CALL(*mock_profile_client_, GetProperties(_, _, _)).WillByDefault(
        Invoke(&ManagedNetworkConfigurationHandlerShutdownTest::GetProperties));
  }

  static void GetProperties(
      const dbus::ObjectPath& profile_path,
      const ShillClientHelper::DictionaryValueCallbackWithoutStatus& callback,
      const ShillClientHelper::ErrorCallback& error_callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(ManagedNetworkConfigurationHandlerShutdownTest::
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
  managed_network_configuration_handler_->RemoveObserver(&policy_observer_);
  managed_network_configuration_handler_.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
