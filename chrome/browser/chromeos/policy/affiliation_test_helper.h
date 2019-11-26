// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATION_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATION_TEST_HELPER_H_

#include <set>
#include <string>

#include "base/macros.h"
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

class AffiliationTestHelper {
 public:
  // Creates an |AffiliationTestHelper| for Cloud management (regular Google
  // accounts). The |fake_session_manager_client| pointer must outlive this
  // object.
  static AffiliationTestHelper CreateForCloud(
      chromeos::FakeSessionManagerClient* fake_session_manager_client);

  // Creates an |AffiliationTestHelper| for Active Directory management (Active
  // Directory accounts). The pointers must outlive this object.
  static AffiliationTestHelper CreateForActiveDirectory(
      chromeos::FakeSessionManagerClient* fake_session_manager_client,
      chromeos::FakeAuthPolicyClient* fake_authpolicy_client);

  // Allow move construction, so the static constructors can be used despite
  // DISALLOW_COPY_AND_ASSIGN.
  AffiliationTestHelper(AffiliationTestHelper&& other);

  // Sets device affiliation IDs to |device_affiliation_ids| in
  // |fake_session_manager_client_| and modifies |test_helper| so that it
  // contains correct values of device affiliation IDs for future use. To add
  // some device policies and have device affiliation ID valid use |test_helper|
  // modified by this function.
  void SetDeviceAffiliationIDs(
      policy::DevicePolicyCrosTestHelper* test_helper,
      const std::set<std::string>& device_affiliation_ids);

  // Sets user affiliation IDs to |user_affiliation_ids| in
  // |fake_session_manager_client| and modifies |user_policy| so that it
  // contains correct values of user affiliation IDs for future use. To add user
  // policies and have user affiliation IDs valid please use |user_policy|
  // modified by this function.
  void SetUserAffiliationIDs(policy::UserPolicyBuilder* user_policy,
                             const AccountId& user_account_id,
                             const std::set<std::string>& user_affiliation_ids);

  // Registers the user with the given |account_id| on the device and marks OOBE
  // as completed. This method should be called in PRE_* test.
  static void PreLoginUser(const AccountId& account_id);

  // Log in user with |account_id|. User should be registered using
  // PreLoginUser().
  static void LoginUser(const AccountId& user_id);

  // Set necessary for login command line switches. Execute it in
  // SetUpCommandLine().
  static void AppendCommandLineSwitchesForLoginManager(
      base::CommandLine* command_line);

  static const char kFakeRefreshToken[];
  static const char kEnterpriseUserEmail[];
  static const char kEnterpriseUserGaiaId[];

 private:
  enum class ManagementType { kCloud, kActiveDirectory };

  AffiliationTestHelper(
      ManagementType management_type,
      chromeos::FakeSessionManagerClient* fake_session_manager_client,
      chromeos::FakeAuthPolicyClient* fake_authpolicy_client);

  // ASSERTs on pointer validity.
  void CheckPreconditions();

  ManagementType management_type_;
  chromeos::FakeSessionManagerClient*
      fake_session_manager_client_;                          // Not owned.
  chromeos::FakeAuthPolicyClient* fake_authpolicy_client_;   // Not owned.

  DISALLOW_COPY_AND_ASSIGN(AffiliationTestHelper);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATION_TEST_HELPER_H_
