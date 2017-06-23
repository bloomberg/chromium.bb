// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_ACCOUNT_TRACKER_H_
#define COMPONENTS_GCM_DRIVER_GCM_ACCOUNT_TRACKER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "google_apis/gaia/account_tracker.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace base {
class Time;
}

namespace gcm {

class GCMDriver;

// Class for reporting back which accounts are signed into. It is only meant to
// be used when the user is signed into sync.
//
// This class makes a check for tokens periodically, to make sure the user is
// still logged into the profile, so that in the case that the user is not, we
// can immediately report that to the GCM and stop messages addressed to that
// user from ever reaching Chrome.
class GCMAccountTracker : public gaia::AccountTracker::Observer,
                          public OAuth2TokenService::Consumer,
                          public GCMConnectionObserver {
 public:
  // State of the account.
  // Allowed transitions:
  // TOKEN_NEEDED - account info was created.
  // TOKEN_NEEDED -> GETTING_TOKEN - access token was requested.
  // GETTING_TOKEN -> TOKEN_NEEDED - access token fetching failed.
  // GETTING_TOKEN -> TOKEN_PRESENT - access token fetching succeeded.
  // GETTING_TOKEN -> ACCOUNT_REMOVED - account was removed.
  // TOKEN_NEEDED -> ACCOUNT_REMOVED - account was removed.
  // TOKEN_PRESENT -> ACCOUNT_REMOVED - account was removed.
  enum AccountState {
    TOKEN_NEEDED,     // Needs a token (AccountInfo was recently created or
                      // token request failed).
    GETTING_TOKEN,    // There is a pending token request.
    TOKEN_PRESENT,    // We have a token for the account.
    ACCOUNT_REMOVED,  // Account was removed, and we didn't report it yet.
  };

  // Stores necessary account information and state of token fetching.
  struct AccountInfo {
    AccountInfo(const std::string& email, AccountState state);
    ~AccountInfo();

    // Email address of the tracked account.
    std::string email;
    // OAuth2 access token, when |state| is TOKEN_PRESENT.
    std::string access_token;
    // Expiration time of the access tokens.
    base::Time expiration_time;
    // Status of the token fetching.
    AccountState state;
  };

  // |account_tracker| is used to deliver information about the accounts present
  // in the browser context to |driver|.
  GCMAccountTracker(std::unique_ptr<gaia::AccountTracker> account_tracker,
                    GCMDriver* driver);
  ~GCMAccountTracker() override;

  // Shuts down the tracker ensuring a proper clean up. After Shutdown() is
  // called Start() and Stop() should no longer be used. Must be called before
  // destruction.
  void Shutdown();

  // Starts tracking accounts.
  void Start();

  // Gets the number of pending token requests. Only used for testing.
  size_t get_pending_token_request_count() const {
    return pending_token_requests_.size();
  }

 private:
  friend class GCMAccountTrackerTest;

  // Maps account keys to account states. Keyed by account_ids as used by
  // OAuth2TokenService.
  typedef std::map<std::string, AccountInfo> AccountInfos;

  // AccountTracker::Observer overrides.
  void OnAccountSignInChanged(const gaia::AccountIds& ids,
                              bool is_signed_in) override;

  // OAuth2TokenService::Consumer overrides.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // GCMConnectionObserver overrides.
  void OnConnected(const net::IPEndPoint& ip_endpoint) override;
  void OnDisconnected() override;

  // Schedules token reporting.
  void ScheduleReportTokens();
  // Report the list of accounts with OAuth2 tokens back using the |callback_|
  // function. If there are token requests in progress, do nothing.
  void ReportTokens();
  // Verify that all of the tokens are ready to be passed down to the GCM
  // Driver, e.g. none of them has expired or is missing. Returns true if not
  // all tokens are valid and a fetching yet more tokens is required.
  void SanitizeTokens();
  // Indicates whether token reporting is required, either because it is due, or
  // some of the accounts were removed.
  bool IsTokenReportingRequired() const;
  // Indicates whether there are tokens that still need fetching.
  bool IsTokenFetchingRequired() const;
  // Gets the time until next token reporting.
  base::TimeDelta GetTimeToNextTokenReporting() const;
  // Deletes a token request. Should be called from OnGetTokenSuccess(..) or
  // OnGetTokenFailure(..).
  void DeleteTokenRequest(const OAuth2TokenService::Request* request);
  // Checks on all known accounts, and calls GetToken(..) for those with
  // |state == TOKEN_NEEDED|.
  void GetAllNeededTokens();
  // Starts fetching the OAuth2 token for the GCM group scope.
  void GetToken(AccountInfos::iterator& account_iter);

  // Handling of actual sign in and sign out for accounts.
  void OnAccountSignedIn(const gaia::AccountIds& ids);
  void OnAccountSignedOut(const gaia::AccountIds& ids);

  OAuth2TokenService* GetTokenService();

  // Account tracker.
  std::unique_ptr<gaia::AccountTracker> account_tracker_;

  // GCM Driver. Not owned.
  GCMDriver* driver_;

  // State of the account.
  AccountInfos account_infos_;

  // Indicates whether shutdown has been called.
  bool shutdown_called_;

  std::vector<std::unique_ptr<OAuth2TokenService::Request>>
      pending_token_requests_;

  // Creates weak pointers used to postpone reporting tokens. See
  // ScheduleReportTokens.
  base::WeakPtrFactory<GCMAccountTracker> reporting_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMAccountTracker);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_ACCOUNT_TRACKER_H_
