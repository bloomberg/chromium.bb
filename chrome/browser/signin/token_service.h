// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The TokenService will supply authentication tokens for any service that
// needs it, such as sync. Whenever the user logs in, a controller watching
// the token service is expected either to call ClientLogin to derive a new
// SID and LSID, or to use GAIA OAuth requests to derive an OAuth1 access
// token for the OAuthLogin scope.  Whenever such credentials are available,
// the TokenService should be updated with new credentials.  The controller
// should then start fetching tokens, which will be written to the database
// after retrieval, as well as provided to listeners.
//
// A token service controller like the ChromiumOS login is expected to:
//
// Initialize()  // Soon as you can
// LoadTokensFromDB()  // When it's OK to talk to the database
// UpdateCredentials()  // When user logs in
// StartFetchingTokens()  // When it's safe to start fetching
//
// Typically a user of the TokenService is expected just to call:
//
// if (token_service.HasTokenForService(servicename)) {
//   SetMyToken(token_service.GetTokenForService(servicename));
// }
// RegisterSomeObserver(token_service);
//
// Whenever a token update occurs:
// OnTokenAvailable(...) {
//   if (IsServiceICareAbout(notification.service())) {
//     SetMyToken(notification.token())
//   }
// }
//
// There is currently no easy way to create a fake TokenService. Tests that want
// to use TokenService to issue tokens without the use of fake GaiaAuthFetchers
// or persisting the tokens to disk via WebDataService can do this by
// creating a TokenService (skipping the Initialize() step to avoid interacting
// with WebDataService) and calling IssueAuthTokenForTest() to issue new tokens.
// This will result in the TokenService sending out the appropriate
// TOKEN_AVAILABLE notification and returning the correct response to future
// calls to Has/GetTokenForService().

#ifndef CHROME_BROWSER_SIGNIN_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_TOKEN_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/signin/signin_internals_util.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"

class Profile;
class TokenServiceTest;

namespace net {
class URLRequestContextGetter;
}

// The TokenService is a Profile member, so all calls are expected
// from the UI thread.
class TokenService : public GaiaAuthConsumer,
                     public ProfileKeyedService,
                     public WebDataServiceConsumer {
 public:
   TokenService();
   virtual ~TokenService();

  // Notification classes
  class TokenAvailableDetails {
   public:
    TokenAvailableDetails() {}
    TokenAvailableDetails(const std::string& service,
                          const std::string& token)
        : service_(service), token_(token) {}
    const std::string& service() const { return service_; }
    const std::string& token() const { return token_; }
   private:
    std::string service_;
    std::string token_;
  };

  class TokenRequestFailedDetails {
   public:
    TokenRequestFailedDetails()
        : error_(GoogleServiceAuthError::NONE) {}
    TokenRequestFailedDetails(const std::string& service,
                              const GoogleServiceAuthError& error)
        : service_(service), error_(error) {}
    const std::string& service() const { return service_; }
    const GoogleServiceAuthError& error() const { return error_; }
   private:
    std::string service_;
    GoogleServiceAuthError error_;
  };

  // Methods to register or remove SigninDiagnosticObservers
  void AddSigninDiagnosticsObserver(
      signin_internals_util::SigninDiagnosticsObserver* observer);
  void RemoveSigninDiagnosticsObserver(
      signin_internals_util::SigninDiagnosticsObserver* observer);

  // Initialize this token service with a request source
  // (usually from a GaiaAuthConsumer constant), and the profile.
  // Typically you'd then update the credentials.
  void Initialize(const char* const source, Profile* profile);

  // Used to determine whether Initialize() has been called.
  bool Initialized() const { return !source_.empty(); }

  // Add a token not supported by a fetcher.
  void AddAuthTokenManually(const std::string& service,
                            const std::string& auth_token);

  // Update ClientLogin credentials in the token service.
  // Afterwards you can StartFetchingTokens.
  void UpdateCredentials(
      const GaiaAuthConsumer::ClientLoginResult& credentials);

  // Update credentials in the token service with oauth2 tokens.
  // Afterwards you can StartFetchingTokens.
  void UpdateCredentialsWithOAuth2(
      const GaiaAuthConsumer::ClientOAuthResult& credentials);

  // Terminate any running requests and reset the TokenService to a clean
  // slate. Resets in memory structures. Does not modify the DB.
  // When this is done, no tokens will be left in memory and no
  // user credentials will be left. Useful if a user is logging out.
  // Initialize doesn't need to be called again but UpdateCredentials does.
  void ResetCredentialsInMemory();

  // Async load all tokens for services we know of from the DB.
  // You should do this at startup. Optionally you can do it again
  // after you reset in memory credentials.
  virtual void LoadTokensFromDB();

  // Clear all DB stored tokens for the current profile. Tokens may still be
  // available in memory. If a DB load is pending it may still be serviced.
  void EraseTokensFromDB();

  // Returns true if tokens have been loaded from the DB. Set when
  // LoadTokensFromDB() completes, unset when ResetCredentialsInMemory() is
  // called.
  bool TokensLoadedFromDB() const;

  // Returns true if the token service has either GAIA credentials or OAuth2
  // tokens needed to fetch other service tokens.
  virtual bool AreCredentialsValid() const;

  // Tokens will be fetched for all services(sync, talk) in the background.
  // Results come back via event channel. Services can also poll before events
  // are issued.
  void StartFetchingTokens();
  virtual bool HasTokenForService(const char* service) const;
  const std::string& GetTokenForService(const char* const service) const;

  // OAuth login token is an all-powerful token that allows creating OAuth2
  // tokens for any other scope (i.e. down-scoping).
  // Typical use is to create an OAuth2 token for appropriate scope and then
  // use that token to call a Google API.
  virtual bool HasOAuthLoginToken() const;
  virtual const std::string& GetOAuth2LoginRefreshToken() const;

  // For tests only. Doesn't save to the WebDB.
  void IssueAuthTokenForTest(const std::string& service,
                             const std::string& auth_token);

  // GaiaAuthConsumer implementation.
  virtual void OnIssueAuthTokenSuccess(const std::string& service,
                                       const std::string& auth_token) OVERRIDE;
  virtual void OnIssueAuthTokenFailure(
      const std::string& service,
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnClientOAuthSuccess(const ClientOAuthResult& result) OVERRIDE;
  virtual void OnClientOAuthFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(
      WebDataService::Handle h,
      const WDTypedResult* result) OVERRIDE;

 protected:
  // Saves OAuth2 credentials.
  void SaveOAuth2Credentials(const ClientOAuthResult& result);

  void set_tokens_loaded(bool loaded) {
    tokens_loaded_ = loaded;
  }

 private:

  // Gets the list of all service names for which tokens will be retrieved.
  // This method is meant only for tests.
  static void GetServiceNamesForTesting(std::vector<std::string>* names);

  void FireTokenAvailableNotification(const std::string& service,
                                      const std::string& auth_token);

  void FireTokenRequestFailedNotification(const std::string& service,
                                          const GoogleServiceAuthError& error);

  void LoadTokensIntoMemory(
      const std::map<std::string, std::string>& db_tokens,
      std::map<std::string, std::string>* in_memory_tokens);
  void LoadSingleTokenIntoMemory(
      const std::map<std::string, std::string>& db_tokens,
      std::map<std::string, std::string>* in_memory_tokens,
      const std::string& service);

  void SaveAuthTokenToDB(const std::string& service,
                         const std::string& auth_token);

  // Returns the index of the given service.
  static int GetServiceIndex(const std::string& service);

  // The profile with which this instance was initialized, or NULL.
  Profile* profile_;

  // Web data service to access tokens from.
  scoped_refptr<WebDataService> web_data_service_;
  // Getter to use for fetchers.
  scoped_refptr<net::URLRequestContextGetter> getter_;
  // Request handle to load Gaia tokens from DB.
  WebDataService::Handle token_loading_query_;
  // True if token loading has completed (regardless of success).
  bool tokens_loaded_;

  // Gaia request source for Gaia accounting.
  std::string source_;
  // Credentials from ClientLogin for Issuing auth tokens.
  GaiaAuthConsumer::ClientLoginResult credentials_;

  // A bunch of fetchers suitable for ClientLogin token issuing. We don't care
  // about the ordering, nor do we care which is for which service.  The
  // number of entries in this array must match the number of entries in the
  // kServices array declared in the cc file.  If not, a compile time error
  // will occur.
  scoped_ptr<GaiaAuthFetcher> fetchers_[2];

  // Map from service to token.
  std::map<std::string, std::string> token_map_;

  // The list of SigninDiagnosticObservers
  ObserverList<signin_internals_util::SigninDiagnosticsObserver>
      signin_diagnostics_observers_;

  friend class TokenServiceTest;
  FRIEND_TEST_ALL_PREFIXES(TokenServiceTest, LoadTokensIntoMemoryBasic);
  FRIEND_TEST_ALL_PREFIXES(TokenServiceTest, LoadTokensIntoMemoryAdvanced);
  FRIEND_TEST_ALL_PREFIXES(TokenServiceTest, FullIntegrationNewServicesAdded);

  DISALLOW_COPY_AND_ASSIGN(TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_TOKEN_SERVICE_H_
