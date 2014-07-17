// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_ACCOUNT_TRACKER_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_ACCOUNT_TRACKER_H_

#include <map>
#include <string>

#include "base/memory/scoped_vector.h"
#include "google_apis/gaia/account_tracker.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace gcm {

// Class for reporting back which accounts are signed into. It is only meant to
// be used when the user is signed into sync.
class GCMAccountTracker : public gaia::AccountTracker::Observer,
                          public OAuth2TokenService::Consumer {
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
    // Status of the token fetching.
    AccountState state;
  };

  // Callback for the GetAccountsForCheckin call. |account_tokens| maps email
  // addresses to OAuth2 access tokens.
  typedef base::Callback<void(const std::map<std::string, std::string>&
                                  account_tokens)> UpdateAccountsCallback;

  // Creates an instance of GCMAccountTracker. |account_tracker| is used to
  // deliver information about the account, while |callback| will be called
  // once all of the accounts have been fetched a necessary OAuth2 token, as
  // many times as the list of accounts is stable, meaning that all accounts
  // are known and there is no related activity in progress for them, like
  // fetching OAuth2 tokens.
  GCMAccountTracker(scoped_ptr<gaia::AccountTracker> account_tracker,
                    const UpdateAccountsCallback& callback);
  virtual ~GCMAccountTracker();

  // Shuts down the tracker ensuring a proper clean up. After Shutdown() is
  // called Start() and Stop() should no longer be used. Must be called before
  // destruction.
  void Shutdown();

  // Starts tracking accounts.
  void Start();
  // Stops tracking accounts. Cancels all of the pending token requests.
  void Stop();

 private:
  // Maps account keys to account states. Keyed by account_ids as used by
  // OAuth2TokenService.
  typedef std::map<std::string, AccountInfo> AccountInfos;

  // AccountTracker::Observer overrides.
  virtual void OnAccountAdded(const gaia::AccountIds& ids) OVERRIDE;
  virtual void OnAccountRemoved(const gaia::AccountIds& ids) OVERRIDE;
  virtual void OnAccountSignInChanged(const gaia::AccountIds& ids,
                                      bool is_signed_in) OVERRIDE;

  // OAuth2TokenService::Consumer overrides.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // Report the list of accounts with OAuth2 tokens back using the |callback_|
  // function. If there are token requests in progress, do nothing.
  void CompleteCollectingTokens();
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
  scoped_ptr<gaia::AccountTracker> account_tracker_;

  // Callback to be called after all of the account and OAuth2 tokens are
  // collected.
  UpdateAccountsCallback callback_;

  // State of the account.
  AccountInfos account_infos_;

  // Indicates whether shutdown has been called.
  bool shutdown_called_;

  ScopedVector<OAuth2TokenService::Request> pending_token_requests_;

  DISALLOW_COPY_AND_ASSIGN(GCMAccountTracker);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_ACCOUNT_TRACKER_H_
