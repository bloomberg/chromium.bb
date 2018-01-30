// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_KNOWN_USER_H_
#define COMPONENTS_USER_MANAGER_KNOWN_USER_H_

#include <string>
#include <vector>

#include "components/user_manager/user_manager_export.h"

class AccountId;
enum class AccountType;
class PrefRegistrySimple;

namespace base {
class DictionaryValue;
}

namespace user_manager {
namespace known_user {
// Methods for storage/retrieval of per-user properties in Local State.

// Performs a lookup of properties associated with |account_id|. If found,
// returns |true| and fills |out_value|. |out_value| can be NULL, if
// only existence check is required.
bool USER_MANAGER_EXPORT FindPrefs(const AccountId& account_id,
                                   const base::DictionaryValue** out_value);

// Updates (or creates) properties associated with |account_id| based
// on |values|. |clear| defines if existing properties are cleared (|true|)
// or if it is just a incremental update (|false|).
void USER_MANAGER_EXPORT UpdatePrefs(const AccountId& account_id,
                                     const base::DictionaryValue& values,
                                     bool clear);

// Returns true if |account_id| preference by |path| does exist,
// fills in |out_value|. Otherwise returns false.
bool USER_MANAGER_EXPORT GetStringPref(const AccountId& account_id,
                                       const std::string& path,
                                       std::string* out_value);

// Updates user's identified by |account_id| string preference |path|.
void USER_MANAGER_EXPORT SetStringPref(const AccountId& account_id,
                                       const std::string& path,
                                       const std::string& in_value);

// Returns true if |account_id| preference by |path| does exist,
// fills in |out_value|. Otherwise returns false.
bool USER_MANAGER_EXPORT GetBooleanPref(const AccountId& account_id,
                                        const std::string& path,
                                        bool* out_value);

// Updates user's identified by |account_id| boolean preference |path|.
void USER_MANAGER_EXPORT SetBooleanPref(const AccountId& account_id,
                                        const std::string& path,
                                        const bool in_value);

// Returns true if |account_id| preference by |path| does exist,
// fills in |out_value|. Otherwise returns false.
bool USER_MANAGER_EXPORT GetIntegerPref(const AccountId& account_id,
                                        const std::string& path,
                                        int* out_value);

// Updates user's identified by |account_id| integer preference |path|.
void USER_MANAGER_EXPORT SetIntegerPref(const AccountId& account_id,
                                        const std::string& path,
                                        const int in_value);

// Returns the list of known AccountIds.
std::vector<AccountId> USER_MANAGER_EXPORT GetKnownAccountIds();

// This call forms full account id of a known user by email and (optionally)
// gaia_id.
// This is a temporary call while migrating to AccountId.
AccountId USER_MANAGER_EXPORT GetAccountId(const std::string& user_email,
                                           const std::string& id,
                                           const AccountType& account_type);

// Returns true if |subsystem| data was migrated to GaiaId for the |account_id|.
bool USER_MANAGER_EXPORT GetGaiaIdMigrationStatus(const AccountId& account_id,
                                                  const std::string& subsystem);

// Marks |subsystem| migrated to GaiaId for the |account_id|.
void USER_MANAGER_EXPORT
SetGaiaIdMigrationStatusDone(const AccountId& account_id,
                             const std::string& subsystem);

// Updates |gaia_id| for user with |account_id|.
// TODO(alemate): Update this once AccountId contains GAIA ID
// (crbug.com/548926).
void USER_MANAGER_EXPORT UpdateGaiaID(const AccountId& account_id,
                                      const std::string& gaia_id);

// Updates |account_id.account_type_| and |account_id.GetGaiaId()| or
// |account_id.GetObjGuid()| for user with |account_id|.
void USER_MANAGER_EXPORT UpdateId(const AccountId& account_id);

// Find GAIA ID for user with |account_id|, fill in |out_value| and return
// true
// if GAIA ID was found or false otherwise.
// TODO(antrim): Update this once AccountId contains GAIA ID
// (crbug.com/548926).
bool USER_MANAGER_EXPORT FindGaiaID(const AccountId& account_id,
                                    std::string* out_value);

// Setter and getter for DeviceId known user string preference.
void USER_MANAGER_EXPORT SetDeviceId(const AccountId& account_id,
                                     const std::string& device_id);

std::string USER_MANAGER_EXPORT GetDeviceId(const AccountId& account_id);

// Setter and getter for GAPSCookie known user string preference.
void USER_MANAGER_EXPORT SetGAPSCookie(const AccountId& account_id,
                                       const std::string& gaps_cookie);

std::string USER_MANAGER_EXPORT GetGAPSCookie(const AccountId& account_id);

// Saves whether the user authenticates using SAML.
void USER_MANAGER_EXPORT UpdateUsingSAML(const AccountId& account_id,
                                         const bool using_saml);

// Returns if SAML needs to be used for authentication of the user with
// |account_id|, if it is known (was set by a |UpdateUsingSaml| call).
// Otherwise
// returns false.
bool USER_MANAGER_EXPORT IsUsingSAML(const AccountId& account_id);

// Returns true if the user's session has already completed initialization
// (set to false when session is created, and then is set to true once
// the profile is intiaiized - this allows us to detect crashes/restarts during
// initial session creation so we can recover gracefully).
bool USER_MANAGER_EXPORT WasProfileEverInitialized(const AccountId& account_id);

// Sets the flag that denotes whether the session associated with a user has
// completed initialization at least once.
void USER_MANAGER_EXPORT SetProfileEverInitialized(const AccountId& account_id,
                                                   bool initialized);

// Enum describing whether a user's profile requires policy. If kPolicyRequired,
// the profile initialization code will ensure that valid policy is loaded
// before session initialization completes.
enum class ProfileRequiresPolicy {
  kUnknown,
  kPolicyRequired,
  kNoPolicyRequired
};

// Returns whether the current profile requires policy or not (returns UNKNOWN
// if the profile has never been initialized and so the policy status is
// not yet known).
ProfileRequiresPolicy USER_MANAGER_EXPORT
GetProfileRequiresPolicy(const AccountId& account_id);

// Sets whether the profile requires policy or not.
void USER_MANAGER_EXPORT
SetProfileRequiresPolicy(const AccountId& account_id,
                         ProfileRequiresPolicy policy_required);

// Saves why the user has to go through re-auth flow.
void USER_MANAGER_EXPORT UpdateReauthReason(const AccountId& account_id,
                                            const int reauth_reason);

// Returns the reason why the user with |account_id| has to go through the
// re-auth flow. Returns true if such a reason was recorded or false
// otherwise.
bool USER_MANAGER_EXPORT FindReauthReason(const AccountId& account_id,
                                          int* out_value);

// Saves that a minimal migration was attempted for this user's cryptohome.
void USER_MANAGER_EXPORT
SetUserHomeMinimalMigrationAttempted(const AccountId& account_id,
                                     bool minimal_migration_attempted);

// Returns true if minimal migration was attempted for this user's cryptohome.
bool USER_MANAGER_EXPORT
WasUserHomeMinimalMigrationAttempted(const AccountId& account_id);

// Removes all user preferences associated with |account_id|.
// Not exported as code should not be calling this outside this component
void RemovePrefs(const AccountId& account_id);

// Clears kProfileEverInitialized for a user.
void USER_MANAGER_EXPORT
RemoveSetProfileEverInitializedPrefForTesting(const AccountId& account_id);

// Register known user prefs.
void USER_MANAGER_EXPORT RegisterPrefs(PrefRegistrySimple* registry);
}
}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_KNOWN_USER_H_
