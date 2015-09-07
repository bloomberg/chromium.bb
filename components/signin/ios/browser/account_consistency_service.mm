// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/account_consistency_service.h"

#include <WebKit/WebKit.h>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_state/web_state_policy_decider.h"
#include "ios/web/public/web_view_creation_util.h"
#include "net/base/mac/url_conversions.h"
#include "url/gurl.h"

namespace {
// JavaScript template used to set (or delete) the X-CHROME-CONNECTED cookie.
// It takes 3 arguments: the value of the cookie, its domain and its expiration
// date.
NSString* const kXChromeConnectedCookieTemplate =
    @"<html><script>document.cookie=\"X-CHROME-CONNECTED=%@; path=/; domain=%@;"
     " expires=\" + new Date(%f).toGMTString();</script></html>";

// WebStatePolicyDecider that monitors the HTTP headers on Gaia responses,
// reacting on the X-Chrome-Manage-Accounts header and notifying its delegate.
class AccountConsistencyHandler : public web::WebStatePolicyDecider {
 public:
  AccountConsistencyHandler(web::WebState* web_state,
                            AccountConsistencyService* service,
                            id<ManageAccountsDelegate> delegate);

 private:
  // web::WebStatePolicyDecider override
  bool ShouldAllowResponse(NSURLResponse* response) override;
  void WebStateDestroyed() override;

  AccountConsistencyService* account_consistency_service_;  // Weak.
  base::WeakNSProtocol<id<ManageAccountsDelegate>> delegate_;
};
}

AccountConsistencyHandler::AccountConsistencyHandler(
    web::WebState* web_state,
    AccountConsistencyService* service,
    id<ManageAccountsDelegate> delegate)
    : web::WebStatePolicyDecider(web_state),
      account_consistency_service_(service),
      delegate_(delegate) {}

bool AccountConsistencyHandler::ShouldAllowResponse(NSURLResponse* response) {
  NSHTTPURLResponse* http_response =
      base::mac::ObjCCast<NSHTTPURLResponse>(response);
  if (!http_response)
    return true;
  if (!gaia::IsGaiaSignonRealm(
          net::GURLWithNSURL(http_response.URL).GetOrigin()))
    return true;
  NSString* manage_accounts_header = [[http_response allHeaderFields]
      objectForKey:@"X-Chrome-Manage-Accounts"];
  if (!manage_accounts_header)
    return true;

  signin::ManageAccountsParams params = signin::BuildManageAccountsParams(
      base::SysNSStringToUTF8(manage_accounts_header));

  switch (params.service_type) {
    case signin::GAIA_SERVICE_TYPE_INCOGNITO: {
      GURL continue_url = GURL(params.continue_url);
      DLOG_IF(ERROR, !params.continue_url.empty() && !continue_url.is_valid())
          << "Invalid continuation URL: \"" << continue_url << "\"";
      [delegate_ onGoIncognito:continue_url];
      break;
    }
    case signin::GAIA_SERVICE_TYPE_SIGNOUT:
    case signin::GAIA_SERVICE_TYPE_ADDSESSION:
    case signin::GAIA_SERVICE_TYPE_REAUTH:
    case signin::GAIA_SERVICE_TYPE_SIGNUP:
    case signin::GAIA_SERVICE_TYPE_DEFAULT:
      [delegate_ onManageAccounts];
      break;
    case signin::GAIA_SERVICE_TYPE_NONE:
      NOTREACHED();
      break;
  }

  // WKWebView loads a blank page even if the response code is 204
  // ("No Content"). http://crbug.com/368717
  //
  // Manage accounts responses are handled via native UI. Abort this request
  // for the following reasons:
  // * Avoid loading a blank page in WKWebView.
  // * Avoid adding this request to history.
  return false;
}

void AccountConsistencyHandler::WebStateDestroyed() {
  account_consistency_service_->RemoveWebStateHandler(web_state());
}

// WKWebView navigation delegate that calls its callback every time a navigation
// has finished.
@interface AccountConsistencyNavigationDelegate : NSObject<WKNavigationDelegate>

// Designated initializer. |callback| will be called every time a navigation has
// finished. |callback| must not be empty.
- (instancetype)initWithCallback:(const base::Closure&)callback
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
@end

@implementation AccountConsistencyNavigationDelegate {
  // Callback that will be called every time a navigation has finished.
  base::Closure _callback;
}

- (instancetype)initWithCallback:(const base::Closure&)callback {
  self = [super init];
  if (self) {
    DCHECK(!callback.is_null());
    _callback = callback;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

#pragma mark - WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  _callback.Run();
}

@end

AccountConsistencyService::CookieRequest
AccountConsistencyService::CookieRequest::CreateAddCookieRequest(
    const std::string& domain) {
  AccountConsistencyService::CookieRequest cookie_request;
  cookie_request.request_type = ADD_COOKIE;
  cookie_request.domain = domain;
  return cookie_request;
}

AccountConsistencyService::CookieRequest
AccountConsistencyService::CookieRequest::CreateRemoveCookieRequest(
    const std::string& domain) {
  AccountConsistencyService::CookieRequest cookie_request;
  cookie_request.request_type = REMOVE_COOKIE;
  cookie_request.domain = domain;
  return cookie_request;
}

AccountConsistencyService::AccountConsistencyService(
    web::BrowserState* browser_state,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    GaiaCookieManagerService* gaia_cookie_manager_service,
    SigninManager* signin_manager)
    : browser_state_(browser_state),
      cookie_settings_(cookie_settings),
      gaia_cookie_manager_service_(gaia_cookie_manager_service),
      signin_manager_(signin_manager),
      applying_cookie_requests_(false) {
  gaia_cookie_manager_service_->AddObserver(this);
  signin_manager_->AddObserver(this);
  web::BrowserState::GetActiveStateManager(browser_state_)->AddObserver(this);
}

AccountConsistencyService::~AccountConsistencyService() {
  DCHECK(!web_view_);
  DCHECK(!navigation_delegate_);
}

void AccountConsistencyService::SetWebStateHandler(
    web::WebState* web_state,
    id<ManageAccountsDelegate> delegate) {
  DCHECK_EQ(0u, web_state_handlers_.count(web_state));
  web_state_handlers_[web_state].reset(
      new AccountConsistencyHandler(web_state, this, delegate));
}

void AccountConsistencyService::RemoveWebStateHandler(
    web::WebState* web_state) {
  DCHECK_LT(0u, web_state_handlers_.count(web_state));
  web_state_handlers_.erase(web_state);
}

void AccountConsistencyService::Shutdown() {
  gaia_cookie_manager_service_->RemoveObserver(this);
  signin_manager_->RemoveObserver(this);
  web::BrowserState::GetActiveStateManager(browser_state_)
      ->RemoveObserver(this);
  ResetWKWebView();
  web_state_handlers_.clear();
}

void AccountConsistencyService::ApplyCookieRequests() {
  if (applying_cookie_requests_) {
    // A cookie request is already being applied, the following ones will be
    // handled as soon as the current one is done.
    return;
  }
  if (cookie_requests_.empty()) {
    return;
  }
  applying_cookie_requests_ = true;

  const GURL url("https://" + cookie_requests_.front().domain);
  std::string cookie_value = "";
  // Expiration date of the cookie in the JavaScript convention of time, a
  // number of milliseconds since the epoch.
  double expiration_date = 0;
  switch (cookie_requests_.front().request_type) {
    case ADD_COOKIE:
      cookie_value = signin::BuildMirrorRequestCookieIfPossible(
          url, signin_manager_->GetAuthenticatedAccountInfo().gaia,
          cookie_settings_.get(), signin::PROFILE_MODE_DEFAULT);
      // Create expiration date of Now+2y to roughly follow the APISID cookie.
      expiration_date =
          (base::Time::Now() + base::TimeDelta::FromDays(730)).ToJsTime();
      break;
    case REMOVE_COOKIE:
      // Nothing to do. Default values correspond to removing the cookie (no
      // value, expiration date in the past).
      break;
  }
  NSString* html = [NSString
      stringWithFormat:kXChromeConnectedCookieTemplate,
                       base::SysUTF8ToNSString(cookie_value),
                       base::SysUTF8ToNSString(url.host()), expiration_date];
  // Load an HTML string with embedded JavaScript that will set or remove the
  // cookie. By setting the base URL to |url|, this effectively allows to modify
  // cookies on the correct domain without having to do a network request.
  [GetWKWebView() loadHTMLString:html baseURL:net::NSURLWithGURL(url)];
}

void AccountConsistencyService::FinishedApplyingCookieRequest() {
  cookie_requests_.pop_front();
  applying_cookie_requests_ = false;
  ApplyCookieRequests();
}

WKWebView* AccountConsistencyService::GetWKWebView() {
  if (!web::BrowserState::GetActiveStateManager(browser_state_)->IsActive()) {
    // |browser_state_| is not active, WKWebView linked to this browser state
    // should not exist or be created.
    return nil;
  }
  if (!web_view_) {
    web_view_.reset(CreateWKWebView());
    navigation_delegate_.reset([[AccountConsistencyNavigationDelegate alloc]
        initWithCallback:base::Bind(&AccountConsistencyService::
                                        FinishedApplyingCookieRequest,
                                    base::Unretained(this))]);
    [web_view_ setNavigationDelegate:navigation_delegate_];
  }
  return web_view_.get();
}

WKWebView* AccountConsistencyService::CreateWKWebView() {
  return web::CreateWKWebView(CGRectZero, browser_state_);
}

void AccountConsistencyService::ResetWKWebView() {
  [web_view_ setNavigationDelegate:nil];
  [web_view_ stopLoading];
  web_view_.reset();
  navigation_delegate_.reset();
}

void AccountConsistencyService::AddXChromeConnectedCookies() {
  DCHECK(!browser_state_->IsOffTheRecord());
  cookie_requests_.push_back(
      CookieRequest::CreateAddCookieRequest("google.com"));
  cookie_requests_.push_back(
      CookieRequest::CreateAddCookieRequest("youtube.com"));
  ApplyCookieRequests();
}

void AccountConsistencyService::RemoveXChromeConnectedCookies() {
  DCHECK(!browser_state_->IsOffTheRecord());
  cookie_requests_.push_back(
      CookieRequest::CreateRemoveCookieRequest("google.com"));
  cookie_requests_.push_back(
      CookieRequest::CreateRemoveCookieRequest("youtube.com"));
  ApplyCookieRequests();
}

void AccountConsistencyService::OnAddAccountToCookieCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  AddXChromeConnectedCookies();
}

void AccountConsistencyService::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const GoogleServiceAuthError& error) {
  AddXChromeConnectedCookies();
}

void AccountConsistencyService::GoogleSigninSucceeded(
    const std::string& account_id,
    const std::string& username,
    const std::string& password) {
  AddXChromeConnectedCookies();
}

void AccountConsistencyService::GoogleSignedOut(const std::string& account_id,
                                                const std::string& username) {
  RemoveXChromeConnectedCookies();
}

void AccountConsistencyService::OnActive() {
  // |browser_state_| is now active. There might be some pending cookie requests
  // to apply.
  ApplyCookieRequests();
}

void AccountConsistencyService::OnInactive() {
  // |browser_state_| is now inactive. Stop using |web_view_| and don't create
  // a new one until it is active.
  ResetWKWebView();
}
