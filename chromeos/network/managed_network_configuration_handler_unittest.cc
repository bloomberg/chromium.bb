// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/managed_network_configuration_handler.h"

#include <iostream>
#include <sstream>

#include "base/message_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/dbus/mock_shill_profile_client.h"
#include "chromeos/dbus/mock_shill_service_client.h"
#include "chromeos/dbus/shill_profile_client_stub.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::_;

namespace test_utils = ::chromeos::onc::test_utils;

namespace {

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

}  // namespace


namespace chromeos {

class ManagedNetworkConfigurationHandlerTest : public testing::Test {
 public:
  ManagedNetworkConfigurationHandlerTest() {
  }

  virtual ~ManagedNetworkConfigurationHandlerTest() {
  }

  virtual void SetUp() OVERRIDE {
    MockDBusThreadManager* dbus_thread_manager = new MockDBusThreadManager;
    EXPECT_CALL(*dbus_thread_manager, GetSystemBus())
        .WillRepeatedly(Return(static_cast<dbus::Bus*>(NULL)));
    DBusThreadManager::InitializeForTesting(dbus_thread_manager);

    EXPECT_CALL(*dbus_thread_manager, GetShillManagerClient())
        .WillRepeatedly(Return(&mock_manager_client_));
    EXPECT_CALL(*dbus_thread_manager, GetShillServiceClient())
        .WillRepeatedly(Return(&mock_service_client_));
    EXPECT_CALL(*dbus_thread_manager, GetShillProfileClient())
        .WillRepeatedly(Return(&mock_profile_client_));

    ON_CALL(mock_profile_client_, GetProperties(_,_,_))
        .WillByDefault(Invoke(&stub_profile_client_,
                              &ShillProfileClientStub::GetProperties));

    ON_CALL(mock_profile_client_, GetEntry(_,_,_,_))
        .WillByDefault(Invoke(&stub_profile_client_,
                              &ShillProfileClientStub::GetEntry));

    NetworkConfigurationHandler::Initialize();
    ManagedNetworkConfigurationHandler::Initialize();
    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    ManagedNetworkConfigurationHandler::Shutdown();
    NetworkConfigurationHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

  void SetUpEntry(const std::string& path_to_shill_json,
                  const std::string& profile_path,
                  const std::string& entry_path) {
    stub_profile_client_.AddEntry(
        profile_path,
        entry_path,
        *test_utils::ReadTestDictionary(path_to_shill_json));
  }

  void SetPolicy(onc::ONCSource onc_source,
                 const std::string& path_to_onc) {
    scoped_ptr<base::DictionaryValue> policy;
    if (path_to_onc.empty())
      policy = onc::ReadDictionaryFromJson(onc::kEmptyUnencryptedConfiguration);
    else
      policy = test_utils::ReadTestDictionary(path_to_onc);

    base::ListValue* network_configs = NULL;
    policy->GetListWithoutPathExpansion(
        onc::toplevel_config::kNetworkConfigurations, &network_configs);

    managed_handler()->SetPolicy(onc::ONC_SOURCE_USER_POLICY, *network_configs);
  }

  ManagedNetworkConfigurationHandler* managed_handler() {
    return ManagedNetworkConfigurationHandler::Get();
  }

 protected:
  StrictMock<MockShillManagerClient> mock_manager_client_;
  StrictMock<MockShillServiceClient> mock_service_client_;
  StrictMock<MockShillProfileClient> mock_profile_client_;
  ShillProfileClientStub stub_profile_client_;
  MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedNetworkConfigurationHandlerTest);
};

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnconfigured) {
  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  EXPECT_CALL(mock_profile_client_,
              GetProperties(dbus::ObjectPath("/profile/chronos/shill"),
                            _, _)).Times(1);

  EXPECT_CALL(mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath("/profile/chronos/shill"),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _)).Times(1);

  SetPolicy(onc::ONC_SOURCE_USER_POLICY, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmodified) {
  EXPECT_CALL(mock_profile_client_,
              GetProperties(_, _, _)).Times(1);

  EXPECT_CALL(mock_manager_client_,
              ConfigureServiceForProfile(_, _, _, _)).Times(1);

  SetPolicy(onc::ONC_SOURCE_USER_POLICY, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&mock_profile_client_);
  Mock::VerifyAndClearExpectations(&mock_manager_client_);

  SetUpEntry("policy/shill_policy_on_unmanaged_user_wifi1.json",
             "/profile/chronos/shill",
             "some_entry_path");

  EXPECT_CALL(mock_profile_client_,
              GetProperties(_, _, _)).Times(1);

  EXPECT_CALL(mock_profile_client_,
              GetEntry(dbus::ObjectPath("/profile/chronos/shill"),
                       "some_entry_path",
                       _, _)).Times(1);

  SetPolicy(onc::ONC_SOURCE_USER_POLICY, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnmanaged) {
  SetUpEntry("policy/shill_unmanaged_user_wifi1.json",
             "/profile/chronos/shill",
             "old_entry_path");

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_user_wifi1.json");

  EXPECT_CALL(mock_profile_client_,
              GetProperties(dbus::ObjectPath("/profile/chronos/shill"),
                            _, _)).Times(1);

  EXPECT_CALL(mock_profile_client_,
              GetEntry(dbus::ObjectPath("/profile/chronos/shill"),
                       "old_entry_path",
                       _, _)).Times(1);

  EXPECT_CALL(mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath("/profile/chronos/shill"),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _)).Times(1);

  SetPolicy(onc::ONC_SOURCE_USER_POLICY, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedNewGUID) {
  SetUpEntry("policy/shill_managed_wifi1.json",
             "/profile/chronos/shill",
             "old_entry_path");

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_user_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't sent it on GetProperties calls.
  expected_shill_properties->RemoveWithoutPathExpansion(
      flimflam::kPassphraseProperty, NULL);

  EXPECT_CALL(mock_profile_client_,
              GetProperties(dbus::ObjectPath("/profile/chronos/shill"),
                            _, _)).Times(1);

  EXPECT_CALL(mock_profile_client_,
              GetEntry(dbus::ObjectPath("/profile/chronos/shill"),
                       "old_entry_path",
                       _, _)).Times(1);

  EXPECT_CALL(mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath("/profile/chronos/shill"),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _)).Times(1);

  SetPolicy(onc::ONC_SOURCE_USER_POLICY, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyReapplyToManaged) {
  SetUpEntry("policy/shill_policy_on_unmanaged_user_wifi1.json",
             "/profile/chronos/shill",
             "old_entry_path");

  scoped_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_user_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't sent it on GetProperties calls.
  expected_shill_properties->RemoveWithoutPathExpansion(
      flimflam::kPassphraseProperty, NULL);

  EXPECT_CALL(mock_profile_client_,
              GetProperties(dbus::ObjectPath("/profile/chronos/shill"),
                            _, _)).Times(1);

  EXPECT_CALL(mock_profile_client_,
              GetEntry(dbus::ObjectPath("/profile/chronos/shill"),
                       "old_entry_path",
                       _, _)).Times(1);

  EXPECT_CALL(mock_manager_client_,
              ConfigureServiceForProfile(
                  dbus::ObjectPath("/profile/chronos/shill"),
                  IsEqualTo(expected_shill_properties.get()),
                  _, _)).Times(1);

  SetPolicy(onc::ONC_SOURCE_USER_POLICY, "policy/policy_wifi1.onc");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUnmanageManaged) {
  SetUpEntry("policy/shill_policy_on_unmanaged_user_wifi1.json",
             "/profile/chronos/shill",
             "old_entry_path");

  EXPECT_CALL(mock_profile_client_,
              GetProperties(dbus::ObjectPath("/profile/chronos/shill"),
                            _, _)).Times(1);

  EXPECT_CALL(mock_profile_client_,
              GetEntry(dbus::ObjectPath("/profile/chronos/shill"),
                       "old_entry_path",
                       _, _)).Times(1);

  EXPECT_CALL(mock_profile_client_,
              DeleteEntry(dbus::ObjectPath("/profile/chronos/shill"),
                          "old_entry_path",
                          _, _)).Times(1);

  SetPolicy(onc::ONC_SOURCE_USER_POLICY, "");
  message_loop_.RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmanaged) {
  SetUpEntry("policy/shill_unmanaged_user_wifi1.json",
             "/profile/chronos/shill",
             "old_entry_path");

  EXPECT_CALL(mock_profile_client_,
              GetProperties(dbus::ObjectPath("/profile/chronos/shill"),
                            _, _)).Times(1);

  EXPECT_CALL(mock_profile_client_,
              GetEntry(dbus::ObjectPath("/profile/chronos/shill"),
                       "old_entry_path",
                       _, _)).Times(1);

  SetPolicy(onc::ONC_SOURCE_USER_POLICY, "");
  message_loop_.RunUntilIdle();
}

}  // namespace chromeos
