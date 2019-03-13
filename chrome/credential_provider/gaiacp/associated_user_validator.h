// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_ASSOCIATED_USER_VALIDATOR_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_ASSOCIATED_USER_VALIDATOR_H_

#include <credentialprovider.h>

#include <map>
#include <set>

#include <base/strings/string16.h>
#include <base/time/time.h>
#include <base/win/scoped_handle.h>

#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"

namespace credential_provider {

// Caches the current validity of token handles and updates the validity if
// it is older than a specified validity lifetime.
class AssociatedUserValidator {
 public:
  // Default timeout when querying token info for token handles. If a timeout
  // occurs the token handle is assumed to be valid.
  static const base::TimeDelta kDefaultTokenHandleValidationTimeout;

  // Minimum time between token handle info refreshes. When trying to get token
  // info, if the info is older than this value, a new token info query will
  // be made.
  static const base::TimeDelta kTokenHandleValidityLifetime;

  // Default URL used to fetch token info for token handles.
  static const char kTokenInfoUrl[];

  static AssociatedUserValidator* Get();

  // Get all the token handles for all associated users and start queries
  // for their validity. The queries are fired in separate threads but
  // no wait is done for the result. This allows background processing of
  // the queries until they are actually needed. An eventual call to
  // IsTokenHandleValidForUser will cause the wait for the result as needed.
  void StartRefreshingTokenHandleValidity();

  // Checks whether the token handle for the given user is valid or not.
  // This function is blocking and may fire off a query for a token handle that
  // needs to complete before the function returns.
  bool IsTokenHandleValidForUser(const base::string16& sid);

  // Checks if user access blocking is enforced given the usage scenario (and
  // other registry based checks).
  bool IsUserAccessBlockingEnforced(
      CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) const;

  // Goes through all associated users found and denies their access to sign
  // in to the system based on the validity of their token handle.
  void DenySigninForUsersWithInvalidTokenHandles(
      CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);

  // Restores the access for a user that was denied access (if applicable).
  // Returns S_OK on success, failure otherwise.
  HRESULT RestoreUserAccess(const base::string16& sid);

  // Allows access for all users that have had their access denied by this
  // token validator.
  void AllowSigninForUsersWithInvalidTokenHandles();

  // Restores access to all associated users. Regardless of their access
  // state. This ensures that no user can be completely locked out due
  // a bad computer state or crash.
  void AllowSigninForAllAssociatedUsers(
      CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus);

  // Fills |associated_sids| with the sids of all valid associated users
  // found on this system.
  std::set<base::string16> GetUpdatedAssociatedSids();
  size_t GetAssociatedUsersCount() { return GetUpdatedAssociatedSids().size(); }

 protected:
  explicit AssociatedUserValidator(base::TimeDelta validation_timeout);
  virtual ~AssociatedUserValidator();

  bool HasInternetConnection() const;
  void CheckTokenHandleValidity(
      const std::map<base::string16, base::string16>& handles_to_verify);
  void StartTokenValidityQuery(const base::string16& sid,
                               const base::string16& token_handle,
                               base::TimeDelta timeout);
  HRESULT UpdateAssociatedSids(
      std::map<base::string16, base::string16>* sid_to_handle);

  // Returns the storage used for the instance pointer.
  static AssociatedUserValidator** GetInstanceStorage();

  // Stores information about the current state of a user's token handle.
  // This information includes:
  //   * The last token handle found for the user.
  //   * The validity of this last token handle (if checked).
  //   * The time of the last update of the validity of this token handle.
  //     This will often be the max end time of the last query that was made on
  //     the token handle.
  //   * The handle to the current thread being executed to verify the
  //     validity of the last token handle.
  struct TokenHandleInfo {
    TokenHandleInfo();
    ~TokenHandleInfo();

    // Used when the handle is empty or invalid.
    explicit TokenHandleInfo(const base::string16& token_handle);

    // Used to create a new token handle info that needs to query validity.
    // The validity is assumed to be invalid at the time of construction.
    TokenHandleInfo(const base::string16& token_handle,
                    base::Time update_time,
                    base::win::ScopedHandle::Handle thread_handle);

    base::string16 queried_token_handle;
    bool is_valid = false;
    base::Time last_update;
    base::win::ScopedHandle pending_query_thread;
  };

  // Maps a user's sid to the token handle info associated with this user (if
  // any).
  std::map<base::string16, std::unique_ptr<TokenHandleInfo>>
      user_to_token_handle_info_;
  base::TimeDelta validation_timeout_;
  std::set<base::string16> locked_user_sids_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_ASSOCIATED_USER_VALIDATOR_H_
