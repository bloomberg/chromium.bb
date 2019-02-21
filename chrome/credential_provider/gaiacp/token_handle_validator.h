// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_TOKEN_HANDLE_VALIDATOR_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_TOKEN_HANDLE_VALIDATOR_H_

#include <map>

#include <base/strings/string16.h>
#include <base/time/time.h>
#include <base/win/scoped_handle.h>

#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"

namespace credential_provider {

// Caches the current validity of token handles and updates the validity if
// it is older than a specified validity lifetime.
class TokenHandleValidator {
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

  static TokenHandleValidator* Get();

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

 protected:
  explicit TokenHandleValidator(base::TimeDelta validation_timeout);
  virtual ~TokenHandleValidator();

  bool HasInternetConnection();
  void CheckTokenHandleValidity(
      const std::map<base::string16, base::string16>& handles_to_verify);
  void StartTokenValidityQuery(const base::string16& sid,
                               const base::string16& token_handle,
                               base::TimeDelta timeout);

  // Returns the storage used for the instance pointer.
  static TokenHandleValidator** GetInstanceStorage();

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
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_TOKEN_HANDLE_VALIDATOR_H_
