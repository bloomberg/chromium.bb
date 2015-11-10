// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_ACCOUNT_CONSISTENCY_SERVICE_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_ACCOUNT_CONSISTENCY_SERVICE_H_

#include <deque>
#include <map>
#include <set>
#include <string>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_manager.h"
#import "components/signin/ios/browser/manage_accounts_delegate.h"
#include "ios/web/public/active_state_manager.h"

namespace web {
class BrowserState;
class WebState;
class WebStatePolicyDecider;
}

class AccountReconcilor;
class SigninClient;

@class AccountConsistencyNavigationDelegate;
@class WKWebView;

// Handles actions necessary for Account Consistency to work on iOS. This
// includes setting the Account Consistency cookie (informing Gaia that the
// Account Consistency is on).
//
// This is currently only used when WKWebView is enabled.
class AccountConsistencyService : public KeyedService,
                                  public GaiaCookieManagerService::Observer,
                                  public SigninManagerBase::Observer,
                                  public web::ActiveStateManager::Observer {
 public:
  // Name of the preference property that persists the domains that have a
  // X-CHROME-CONNECTED cookie set by this service.
  static const char kDomainsWithCookiePref[];

  AccountConsistencyService(
      web::BrowserState* browser_state,
      AccountReconcilor* account_reconcilor,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      GaiaCookieManagerService* gaia_cookie_manager_service,
      SigninClient* signin_client,
      SigninManager* signin_manager);
  ~AccountConsistencyService() override;

  // Registers the preferences used by AccountConsistencyService.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // Sets the handler for |web_state| that reacts on Gaia responses with the
  // X-Chrome-Manage-Accounts header and notifies |delegate|.
  void SetWebStateHandler(web::WebState* web_state,
                          id<ManageAccountsDelegate> delegate);
  // Removes the handler associated with |web_state|.
  void RemoveWebStateHandler(web::WebState* web_state);

  // Enqueues a request to add the X-CHROME-CONNECTED cookie to |domain|. Does
  // nothing if the cookie is already on |domain|.
  void AddXChromeConnectedCookieToDomain(const std::string& domain);

  // Enqueues a request to remove the X-CHROME-CONNECTED cookie to |domain|.
  // Does nothing if the cookie is not set on |domain|.
  void RemoveXChromeConnectedCookieFromDomain(const std::string& domain);

  // Notifies the AccountConsistencyService that browsing data has been removed.
  void OnBrowsingDataRemoved();

 private:
  friend class AccountConsistencyServiceTest;

  // The type of a cookie request.
  enum CookieRequestType {
    ADD_X_CHROME_CONNECTED_COOKIE,
    REMOVE_X_CHROME_CONNECTED_COOKIE
  };

  // A X-CHROME-CONNECTED cookie request to be applied by the
  // AccountConsistencyService.
  struct CookieRequest {
    static CookieRequest CreateAddCookieRequest(const std::string& domain);
    static CookieRequest CreateRemoveCookieRequest(const std::string& domain);
    CookieRequestType request_type;
    std::string domain;
  };

  // Loads the domains with a X-CHROME-CONNECTED cookie from the prefs.
  void LoadFromPrefs();

  // KeyedService implementation.
  void Shutdown() override;

  // Applies the pending X-CHROME-CONNECTED cookie requests one by one.
  void ApplyCookieRequests();

  // Called when the current X-CHROME-CONNECTED cookie request is done.
  void FinishedApplyingCookieRequest(bool success);

  // Returns the cached WKWebView if it exists, or creates one if necessary.
  // Can return nil if the browser state is not active.
  WKWebView* GetWKWebView();
  // Actually creates a WKWebView. Virtual for testing.
  virtual WKWebView* CreateWKWebView();
  // Stops any page loading in the WKWebView currently in use and releases it.
  void ResetWKWebView();

  // Adds X-CHROME-CONNECTED cookies on all the main Google domains.
  void AddXChromeConnectedCookies();
  // Removes X-CHROME-CONNECTED cookies on all the Google domains where it was
  // set.
  void RemoveXChromeConnectedCookies();

  // GaiaCookieManagerService::Observer implementation.
  void OnAddAccountToCookieCompleted(
      const std::string& account_id,
      const GoogleServiceAuthError& error) override;
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& accounts,
      const GoogleServiceAuthError& error) override;

  // SigninManagerBase::Observer implementation.
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username,
                             const std::string& password) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // ActiveStateManager::Observer implementation.
  void OnActive() override;
  void OnInactive() override;

  // Browser state associated with the service, used to create WKWebViews.
  web::BrowserState* browser_state_;
  // Service managing accounts reconciliation, notified of GAIA responses with
  // the X-Chrome-Manage-Accounts header
  AccountReconcilor* account_reconcilor_;
  // Cookie settings currently in use for |browser_state_|, used to check if
  // setting X-CHROME-CONNECTED cookies is valid.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  // Service managing the Gaia cookies, observed to be notified of the state of
  // reconciliation.
  GaiaCookieManagerService* gaia_cookie_manager_service_;
  // Signin client, used to access prefs.
  SigninClient* signin_client_;
  // Signin manager, observed to be notified of signin and signout events.
  SigninManager* signin_manager_;

  // Whether a X-CHROME-CONNECTED cookie request is currently being applied.
  bool applying_cookie_requests_;
  // The queue of X-CHROME-CONNECTED cookie requests to be applied.
  std::deque<CookieRequest> cookie_requests_;
  // The set of domains where a X-CHROME-CONNECTED cookie is present.
  std::set<std::string> domains_with_cookies_;

  // Web view used to apply the X-CHROME-CONNECTED cookie requests.
  base::scoped_nsobject<WKWebView> web_view_;
  // Navigation delegate of |web_view_| that informs the service when a cookie
  // request has been applied.
  base::scoped_nsobject<AccountConsistencyNavigationDelegate>
      navigation_delegate_;

  // Handlers reacting on GAIA responses with the X-Chrome-Manage-Accounts
  // header set.
  std::map<web::WebState*, scoped_ptr<web::WebStatePolicyDecider>>
      web_state_handlers_;

  DISALLOW_COPY_AND_ASSIGN(AccountConsistencyService);
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_ACCOUNT_CONSISTENCY_SERVICE_H_
