// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATION_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATION_TEST_HELPER_H_

#include <set>
#include <string>
#include "components/policy/core/common/cloud/policy_builder.h"

class AccountId;

namespace base {
class CommandLine;
}  // namespace base

namespace chromeos {
class FakeSessionManagerClient;
class FakeAuthPolicyClient;
}  // namespace chromeos

namespace policy {

class DevicePolicyCrosTestHelper;

namespace affiliation_test_helper {

// Creates policy key file for the user specified in |user_policy|.
// TODO(peletskyi): Replace pointer with const reference and replace this
// boilerplate in other places (http://crbug.com/549111).
void SetUserKeys(policy::UserPolicyBuilder* user_policy);

// Sets device affiliation ID to |fake_session_manager_client| from
// |device_affiliation_ids| and modifies |test_helper| so that it contains
// correct values of device affiliation IDs for future use. To add some device
// policies and have device affiliation ID valid please use |test_helper|
// modified by this function. Example:
//
// FakeSessionManagerClient* fake_session_manager_client =
//   new chromeos::FakeSessionManagerClient;
// DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
//   std::unique_ptr<SessionManagerClient>(fake_session_manager_client));
//
// policy::DevicePolicyCrosTestHelper test_helper;
// std::set<std::string> device_affiliation_ids;
// device_affiliation_ids.insert(some-affiliation-id);
//
// affiliation_test_helper::SetDeviceAffiliationIDs(
//     &test_helper,
//     fake_session_manager_client,
//     nullptr /* fake_auth_policy_client */
//     device_affiliation_ids);
//
// If it is used together with SetUserAffiliationIDs() (which is the most common
// case) |fake_session_manager_client| must point to the same object as in
// SetUserAffiliationIDs() call.
// In browser tests one can call this function from
// SetUpInProcessBrowserTestFixture().
//
// |fake_auth_policy_client| is only needed if the test supports Active
// Directory user accounts. If not, it may be nullptr.
void SetDeviceAffiliationIDs(
    policy::DevicePolicyCrosTestHelper* test_helper,
    chromeos::FakeSessionManagerClient* fake_session_manager_client,
    chromeos::FakeAuthPolicyClient* fake_auth_policy_client,
    const std::set<std::string>& device_affiliation_ids);

// Sets user affiliation ID for |user_account_id| to
// |fake_session_manager_client| from |user_affiliation_ids| and modifies
// |user_policy| so that it contains correct values of user affiliation IDs for
// future use. To add user policies and have user affiliation IDs valid please
// use |user_policy| modified by this function. Example:
//
// FakeSessionManagerClient* fake_session_manager_client =
//    new chromeos::FakeSessionManagerClient;
// DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
//    std::unique_ptr<SessionManagerClient>(fake_session_manager_client));
//
// policy::UserPolicyBuilder user_policy;
// std::set<std::string> user_affiliation_ids;
// user_affiliation_ids.insert("some-affiliation-id");
//
// affiliation_test_helper::SetUserAffiliationIDs(
//    &user_policy,
//    fake_session_manager_client,
//    nullptr /* fake_auth_policy_client */,
//    account_id,
//    user_affiliation_ids);
//
// If it is used together SetDeviceAffiliationIDs() (which is the most common
// case) |fake_session_manager_client| must point to the same object as in
// SetDeviceAffiliationIDs() call.
// In browser tests one can call this function from
// SetUpInProcessBrowserTestFixture().
//
// |fake_auth_policy_client| is only needed if the test supports Active
// Directory user accounts. If not, it may be nullptr.
void SetUserAffiliationIDs(
    policy::UserPolicyBuilder* user_policy,
    chromeos::FakeSessionManagerClient* fake_session_manager_client,
    chromeos::FakeAuthPolicyClient* fake_auth_policy_client,
    const AccountId& user_account_id,
    const std::set<std::string>& user_affiliation_ids);

// Registers the user with the given |account_id| on the device and marks OOBE
// as completed. This method should be called in PRE_* test.
void PreLoginUser(const AccountId& account_id);

// Log in user with |account_id|. User should be registered using
// PreLoginUser().
void LoginUser(const AccountId& user_id);

// Set necessary for login command line switches. Execute it in
// SetUpCommandLine().
void AppendCommandLineSwitchesForLoginManager(base::CommandLine* command_line);

extern const char kFakeRefreshToken[];
extern const char kEnterpriseUserEmail[];
extern const char kEnterpriseUserGaiaId[];

}  // namespace affiliation_test_helper

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATION_TEST_HELPER_H_
