// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/oauth2_token_service.h"

class ProfileOAuth2TokenService;
class SigninClient;
class SigninManagerBase;

// Many values in SigninStatus are also associated with a timestamp.
// This makes it easier to keep values and their associated times together.
typedef std::pair<std::string, std::string> TimedSigninStatusValue;

// This class collects authentication, signin and token information
// to propagate to about:signin-internals via SigninInternalsUI.
class AboutSigninInternals
    : public KeyedService,
      public signin_internals_util::SigninDiagnosticsObserver,
      public OAuth2TokenService::DiagnosticsObserver {
 public:
  class Observer {
   public:
    // |info| will contain the dictionary of signin_status_ values as indicated
    // in the comments for GetSigninStatus() below.
    virtual void OnSigninStateChanged(
        scoped_ptr<base::DictionaryValue> info) = 0;
  };

  AboutSigninInternals(ProfileOAuth2TokenService* token_service,
                       SigninManagerBase* signin_manager);
  virtual ~AboutSigninInternals();

  // Each instance of SigninInternalsUI adds itself as an observer to be
  // notified of all updates that AboutSigninInternals receives.
  void AddSigninObserver(Observer* observer);
  void RemoveSigninObserver(Observer* observer);

  // Pulls all signin values that have been persisted in the user prefs.
  void RefreshSigninPrefs();

  // SigninManager::SigninDiagnosticsObserver implementation.
  virtual void NotifySigninValueChanged(
      const signin_internals_util::UntimedSigninStatusField& field,
      const std::string& value) OVERRIDE;

  virtual void NotifySigninValueChanged(
      const signin_internals_util::TimedSigninStatusField& field,
      const std::string& value) OVERRIDE;

  void Initialize(SigninClient* client);

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

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

  // OAuth2TokenService::DiagnosticsObserver implementations.
  virtual void OnAccessTokenRequested(
      const std::string& account_id,
      const std::string& consumer_id,
      const OAuth2TokenService::ScopeSet& scopes) OVERRIDE;
  virtual void OnFetchAccessTokenComplete(
      const std::string& account_id,
      const std::string& consumer_id,
      const OAuth2TokenService::ScopeSet& scopes,
      GoogleServiceAuthError error,
      base::Time expiration_time) OVERRIDE;
  virtual void OnTokenRemoved(const std::string& account_id,
                              const OAuth2TokenService::ScopeSet& scopes)
      OVERRIDE;

    void OnRefreshTokenReceived(std::string status);
    void OnAuthenticationResultReceived(std::string status);

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
    std::vector<std::string> untimed_signin_fields;
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
    scoped_ptr<base::DictionaryValue> ToValue(std::string product_version);
  };

  void NotifyObservers();

  // Weak pointer to the token service.
  ProfileOAuth2TokenService* token_service_;

  // Weak pointer to the signin manager.
  SigninManagerBase* signin_manager_;

  // Weak pointer to the client.
  SigninClient* client_;

  // Encapsulates the actual signin and token related values.
  // Most of the values are mirrored in the prefs for persistence.
  SigninStatus signin_status_;

  ObserverList<Observer> signin_observers_;

  DISALLOW_COPY_AND_ASSIGN(AboutSigninInternals);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ABOUT_SIGNIN_INTERNALS_H_
