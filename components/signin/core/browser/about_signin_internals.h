// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/oauth2_token_service.h"

class AccountTrackerService;
class GaiaAuthFetcher;
class ProfileOAuth2TokenService;
class SigninClient;

// Many values in SigninStatus are also associated with a timestamp.
// This makes it easier to keep values and their associated times together.
typedef std::pair<std::string, std::string> TimedSigninStatusValue;

// This class collects authentication, signin and token information
// to propagate to about:signin-internals via SigninInternalsUI.
class AboutSigninInternals
    : public KeyedService,
      public signin_internals_util::SigninDiagnosticsObserver,
      public OAuth2TokenService::DiagnosticsObserver,
      public GaiaAuthConsumer,
      SigninManagerBase::Observer {
 public:
  class Observer {
   public:
    // |info| will contain the dictionary of signin_status_ values as indicated
    // in the comments for GetSigninStatus() below.
    virtual void OnSigninStateChanged(const base::DictionaryValue* info) = 0;

    // Notification that the cookie accounts are ready to be displayed.
    virtual void OnCookieAccountsFetched(const base::DictionaryValue* info) = 0;
  };

  AboutSigninInternals(ProfileOAuth2TokenService* token_service,
                       AccountTrackerService* account_tracker,
                       SigninManagerBase* signin_manager);
  ~AboutSigninInternals() override;

  // Each instance of SigninInternalsUI adds itself as an observer to be
  // notified of all updates that AboutSigninInternals receives.
  void AddSigninObserver(Observer* observer);
  void RemoveSigninObserver(Observer* observer);

  // Pulls all signin values that have been persisted in the user prefs.
  void RefreshSigninPrefs();

  void Initialize(SigninClient* client);

  void OnRefreshTokenReceived(std::string status);
  void OnAuthenticationResultReceived(std::string status);

  // KeyedService implementation.
  void Shutdown() override;

  // Returns a dictionary of values in signin_status_ for use in
  // about:signin-internals. The values are formatted as shown -
  //
  // { "signin_info" :
  //     [ {"title": "Basic Information",
  //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
  //       },
  //       { "title": "Detailed Information",
  //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
  //       }],
  //   "token_info" :
  //     [ List of {"name": "foo-name", "token" : "foo-token",
  //                 "status": "foo_stat", "time" : "foo_time"} elems]
  //  }
  scoped_ptr<base::DictionaryValue> GetSigninStatus();

  // Triggers a ListAccounts call to acquire a list of the email addresses
  // corresponding to the cookies residing on the current cookie jar.
  void GetCookieAccountsAsync();

 private:
  // Encapsulates diagnostic information about tokens for different services.
  struct TokenInfo {
    TokenInfo(const std::string& consumer_id,
              const OAuth2TokenService::ScopeSet& scopes);
    ~TokenInfo();
    base::DictionaryValue* ToValue() const;

    static bool LessThan(const TokenInfo* a, const TokenInfo* b);

    // Called when the token is invalidated.
    void Invalidate();

    std::string consumer_id;              // service that requested the token.
    OAuth2TokenService::ScopeSet scopes;  // Scoped that are requested.
    base::Time request_time;
    base::Time receive_time;
    base::Time expiration_time;
    GoogleServiceAuthError error;
    bool removed_;
  };

  // Map account id to tokens associated to the account.
  typedef std::map<std::string, std::vector<TokenInfo*> > TokenInfoMap;

  // Encapsulates both authentication and token related information. Used
  // by SigninInternals to maintain information that needs to be shown in
  // the about:signin-internals page.
  struct SigninStatus {
    std::vector<TimedSigninStatusValue> timed_signin_fields;
    TokenInfoMap token_info_map;

    SigninStatus();
    ~SigninStatus();

    TokenInfo* FindToken(const std::string& account_id,
                         const std::string& consumer_id,
                         const OAuth2TokenService::ScopeSet& scopes);

    // Returns a dictionary with the following form:
    // { "signin_info" :
    //     [ {"title": "Basic Information",
    //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
    //       },
    //       { "title": "Detailed Information",
    //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
    //       }],
    //   "token_info" :
    //     [ List of
    //       { "title": account id,
    //         "data": [List of {"service" : service name,
    //                           "scopes" : requested scoped,
    //                           "request_time" : request time,
    //                           "status" : request status} elems]
    //       }],
    //  }
    scoped_ptr<base::DictionaryValue> ToValue(
        AccountTrackerService* account_tracker,
        SigninManagerBase* signin_manager,
        const std::string& product_version);
  };

  // SigninManager::SigninDiagnosticsObserver implementation.
  void NotifySigninValueChanged(
      const signin_internals_util::TimedSigninStatusField& field,
      const std::string& value) override;

  // OAuth2TokenService::DiagnosticsObserver implementations.
  void OnAccessTokenRequested(
      const std::string& account_id,
      const std::string& consumer_id,
      const OAuth2TokenService::ScopeSet& scopes) override;
  void OnFetchAccessTokenComplete(const std::string& account_id,
                                  const std::string& consumer_id,
                                  const OAuth2TokenService::ScopeSet& scopes,
                                  GoogleServiceAuthError error,
                                  base::Time expiration_time) override;
  void OnTokenRemoved(const std::string& account_id,
                      const OAuth2TokenService::ScopeSet& scopes) override;

  // GaiaAuthConsumer implementations.
  void OnListAccountsSuccess(const std::string& data) override;
  void OnListAccountsFailure(const GoogleServiceAuthError& error) override;

  // SigninManagerBase::Observer implementations.
  void GoogleSigninFailed(const GoogleServiceAuthError& error) override;
  void GoogleSigninSucceeded(const std::string& account_id,
                                     const std::string& username,
                                     const std::string& password) override;
  void GoogleSignedOut(const std::string& account_id,
                               const std::string& username) override;

  void NotifyObservers();

  // Callback for ListAccounts. Once the email addresses are fetched from GAIA,
  // they are pushed to the signin_internals_ui.
  void OnListAccountsComplete(
      std::vector<std::pair<std::string, bool> >& gaia_accounts);

  // Called when a cookie changes. If the cookie relates to a GAIA LSID cookie,
  // then we call ListAccounts and update the UI element.
  void OnCookieChanged(const net::CanonicalCookie& cookie, bool removed);

  // Weak pointer to the token service.
  ProfileOAuth2TokenService* token_service_;

  // Weak pointer to the account tracker.
  AccountTrackerService* account_tracker_;

  // Weak pointer to the signin manager.
  SigninManagerBase* signin_manager_;

  // Weak pointer to the client.
  SigninClient* client_;

  // Fetcher for information about accounts in the cookie jar from GAIA.
  scoped_ptr<GaiaAuthFetcher> gaia_fetcher_;

  // Encapsulates the actual signin and token related values.
  // Most of the values are mirrored in the prefs for persistence.
  SigninStatus signin_status_;

  ObserverList<Observer> signin_observers_;

  scoped_ptr<SigninClient::CookieChangedSubscription>
      cookie_changed_subscription_;

  DISALLOW_COPY_AND_ASSIGN(AboutSigninInternals);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_
