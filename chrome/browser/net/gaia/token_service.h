// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The TokenService will supply authentication tokens for any service that
// needs it, such as sync. Whenever the user logs in, a controller watching
// the token service is expected to call ClientLogin to derive a new SID and
// LSID. Whenever such credentials are available, the TokenService should be
// updated with new credentials. The controller should then start fetching
// tokens, which will be written to the database after retrieval, as well as
// provided to listeners.
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

#ifndef CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_H_
#define CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_H_
#pragma once

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Profile;

namespace net {
class URLRequestContextGetter;
}

// The TokenService is a Profile member, so all calls are expected
// from the UI thread.
class TokenService : public GaiaAuthConsumer,
                     public WebDataServiceConsumer,
                     public NotificationObserver {
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

  // Initialize this token service with a request source
  // (usually from a GaiaAuthConsumer constant), and the profile.
  // Typically you'd then update the credentials.
  void Initialize(const char* const source, Profile* profile);

  // Update the credentials in the token service.
  // Afterwards you can StartFetchingTokens.
  void UpdateCredentials(
      const GaiaAuthConsumer::ClientLoginResult& credentials);

  // Terminate any running requests and reset the TokenService to a clean
  // slate. Resets in memory structures. Does not modify the DB.
  // When this is done, no tokens will be left in memory and no
  // user credentials will be left. Useful if a user is logging out.
  // Initialize doesn't need to be called again but UpdateCredentials does.
  void ResetCredentialsInMemory();

  // Async load all tokens for services we know of from the DB.
  // You should do this at startup. Optionally you can do it again
  // after you reset in memory credentials.
  void LoadTokensFromDB();

  // Clear all DB stored tokens for the current profile. Tokens may still be
  // available in memory. If a DB load is pending it may still be serviced.
  void EraseTokensFromDB();

  // For legacy services with their own auth routines, they can just read
  // the LSID out directly. Deprecated.
  bool HasLsid() const;
  const std::string& GetLsid() const;
  // Did we get a proper LSID?
  bool AreCredentialsValid() const;

  // Tokens will be fetched for all services(sync, talk) in the background.
  // Results come back via event channel. Services can also poll before events
  // are issued.
  void StartFetchingTokens();
  bool HasTokenForService(const char* const service) const;
  const std::string& GetTokenForService(const char* const service) const;

  // For tests only. Doesn't save to the WebDB.
  void IssueAuthTokenForTest(const std::string& service,
                             const std::string& auth_token);

  // GaiaAuthConsumer implementation.
  virtual void OnIssueAuthTokenSuccess(const std::string& service,
                                       const std::string& auth_token);
  virtual void OnIssueAuthTokenFailure(const std::string& service,
                                       const GoogleServiceAuthError& error);

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:

  void FireTokenAvailableNotification(const std::string& service,
                                      const std::string& auth_token);

  void FireTokenRequestFailedNotification(const std::string& service,
                                          const GoogleServiceAuthError& error);

  void LoadTokensIntoMemory(const std::map<std::string, std::string>& in_toks,
                            std::map<std::string, std::string>* out_toks);

  void SaveAuthTokenToDB(const std::string& service,
                         const std::string& auth_token);

  // Web data service to access tokens from.
  scoped_refptr<WebDataService> web_data_service_;
  // Getter to use for fetchers.
  scoped_refptr<net::URLRequestContextGetter> getter_;
  // Request handle to load Gaia tokens from DB.
  WebDataService::Handle token_loading_query_;

  // Gaia request source for Gaia accounting.
  std::string source_;
  // Credentials from ClientLogin for Issuing auth tokens.
  GaiaAuthConsumer::ClientLoginResult credentials_;

  // Size of array of services (must be defined here).
  static const int kNumServices = 4;
  // List of services that we're performing operations for.
  static const char* kServices[kNumServices];
  // A bunch of fetchers suitable for token issuing. We don't care about
  // the ordering, nor do we care which is for which service.
  scoped_ptr<GaiaAuthFetcher> fetchers_[kNumServices];
  // Map from service to token.
  std::map<std::string, std::string> token_map_;

  NotificationRegistrar registrar_;

  FRIEND_TEST_ALL_PREFIXES(TokenServiceTest, LoadTokensIntoMemoryBasic);
  FRIEND_TEST_ALL_PREFIXES(TokenServiceTest, LoadTokensIntoMemoryAdvanced);

  DISALLOW_COPY_AND_ASSIGN(TokenService);
};

#endif  // CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_H_
