// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"

#if defined(OS_ANDROID)
#include "chrome/browser/signin/android_profile_oauth2_token_service.h"
#else
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#endif

namespace content {
class BrowserContext;
}

// Helper class to simplify writing unittests that depend on an instance of
// ProfileOAuth2TokenService.
//
// Tests would typically do something like the following:
//
// FakeProfileOAuth2TokenService service;
// ...
// service.IssueRefreshToken("token");  // Issue refresh token/notify observers
// ...
// // Confirm that there is at least one active request.
// EXPECT_GT(0U, service.GetPendingRequests().size());
// ...
// // Make any pending token fetches for a given scope succeed.
// ScopeSet scopes;
// scopes.insert(GaiaConstants::kYourServiceScope);
// IssueTokenForScope(scopes, "access_token", base::Time()::Max());
// ...
// // ...or make them fail...
// IssueErrorForScope(scopes, GoogleServiceAuthError(INVALID_GAIA_CREDENTIALS));
//
class FakeProfileOAuth2TokenService
#if defined(OS_ANDROID)
  : public AndroidProfileOAuth2TokenService {
#else
  : public ProfileOAuth2TokenService {
#endif
 public:
  struct PendingRequest {
    PendingRequest();
    ~PendingRequest();

    std::string account_id;
    std::string client_id;
    std::string client_secret;
    ScopeSet scopes;
    base::WeakPtr<RequestImpl> request;
  };

  FakeProfileOAuth2TokenService();
  virtual ~FakeProfileOAuth2TokenService();

  // Overriden to make sure it works on Android.
  virtual bool RefreshTokenIsAvailable(
      const std::string& account_id) OVERRIDE;

  // Sets the current refresh token. If |token| is non-empty, this will invoke
  // OnRefreshTokenAvailable() on all Observers, otherwise this will invoke
  // OnRefreshTokenRevoked().
  void IssueRefreshToken(const std::string& token);

  // TODO(fgorski,rogerta): Merge with UpdateCredentials when this class fully
  // supports multiple accounts.
  void IssueRefreshTokenForUser(const std::string& account_id,
                                const std::string& token);

  // Gets a list of active requests (can be used by tests to validate that the
  // correct request has been issued).
  std::vector<PendingRequest> GetPendingRequests();

  // Helper routines to issue tokens for pending requests.
  // TODO(fgorski): Add account IDs as parameters.
  void IssueTokenForScope(const ScopeSet& scopes,
                          const std::string& access_token,
                          const base::Time& expiration);

  void IssueErrorForScope(const ScopeSet& scopes,
                          const GoogleServiceAuthError& error);

  void IssueTokenForAllPendingRequests(const std::string& access_token,
                                       const base::Time& expiration);

  void IssueErrorForAllPendingRequests(const GoogleServiceAuthError& error);

  // Helper function to be used with
  // BrowserContextKeyedService::SetTestingFactory().
  static BrowserContextKeyedService* Build(content::BrowserContext* profile);

 protected:
  // OAuth2TokenService overrides.
  virtual void FetchOAuth2Token(RequestImpl* request,
                                const std::string& account_id,
                                net::URLRequestContextGetter* getter,
                                const std::string& client_id,
                                const std::string& client_secret,
                                const ScopeSet& scopes) OVERRIDE;

  virtual std::string GetRefreshToken(const std::string& account_id) OVERRIDE;

  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;

 private:
  // Helper function to complete pending requests - if |all_scopes| is true,
  // then all pending requests are completed, otherwise, only those requests
  // matching |scopes| are completed.
  void CompleteRequests(bool all_scopes,
                        const ScopeSet& scopes,
                        const GoogleServiceAuthError& error,
                        const std::string& access_token,
                        const base::Time& expiration);

  std::vector<PendingRequest> pending_requests_;
  std::string refresh_token_;

  DISALLOW_COPY_AND_ASSIGN(FakeProfileOAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_FAKE_PROFILE_OAUTH2_TOKEN_SERVICE_H_
