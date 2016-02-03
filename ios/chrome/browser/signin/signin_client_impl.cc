// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_client_impl.h"

#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_cookie_changed_subscription.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_provider.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/cookie_settings_factory.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

SigninClientImpl::SigninClientImpl(
    ios::ChromeBrowserState* browser_state,
    SigninErrorController* signin_error_controller)
    : OAuth2TokenService::Consumer("signin_client_impl"),
      browser_state_(browser_state),
      signin_error_controller_(signin_error_controller) {
  signin_error_controller_->AddObserver(this);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

SigninClientImpl::~SigninClientImpl() {
  signin_error_controller_->RemoveObserver(this);
}

void SigninClientImpl::Shutdown() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void SigninClientImpl::DoFinalInit() {
}

// static
bool SigninClientImpl::AllowsSigninCookies(
    ios::ChromeBrowserState* browser_state) {
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      ios::CookieSettingsFactory::GetForBrowserState(browser_state);
  return signin::SettingsAllowSigninCookies(cookie_settings.get());
}

PrefService* SigninClientImpl::GetPrefs() {
  return browser_state_->GetPrefs();
}

scoped_refptr<TokenWebData> SigninClientImpl::GetDatabase() {
  return ios::WebDataServiceFactory::GetTokenWebDataForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
}

bool SigninClientImpl::CanRevokeCredentials() {
  return true;
}

std::string SigninClientImpl::GetSigninScopedDeviceId() {
  return GetOrCreateScopedDeviceIdPref(GetPrefs());
}

void SigninClientImpl::OnSignedOut() {
  BrowserStateInfoCache* cache = GetApplicationContext()
                                     ->GetChromeBrowserStateManager()
                                     ->GetBrowserStateInfoCache();
  size_t index = cache->GetIndexOfBrowserStateWithPath(
      browser_state_->GetOriginalChromeBrowserState()->GetStatePath());

  // If sign out occurs because Sync setup was in progress and the browser state
  // got deleted, then it is no longer in the cache.
  if (index == std::string::npos)
    return;

  cache->SetAuthInfoOfBrowserStateAtIndex(index, std::string(),
                                          base::string16());
}

net::URLRequestContextGetter* SigninClientImpl::GetURLRequestContext() {
  return browser_state_->GetRequestContext();
}

bool SigninClientImpl::ShouldMergeSigninCredentialsIntoCookieJar() {
  return false;
}

std::string SigninClientImpl::GetProductVersion() {
  return GetVersionString();
}

bool SigninClientImpl::IsFirstRun() const {
  return false;
}

base::Time SigninClientImpl::GetInstallDate() {
  return base::Time::FromTimeT(
      GetApplicationContext()->GetMetricsService()->GetInstallDate());
}

bool SigninClientImpl::AreSigninCookiesAllowed() {
  return AllowsSigninCookies(browser_state_);
}

void SigninClientImpl::AddContentSettingsObserver(
    content_settings::Observer* observer) {
  ios::HostContentSettingsMapFactory::GetForBrowserState(browser_state_)
      ->AddObserver(observer);
}

void SigninClientImpl::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
  ios::HostContentSettingsMapFactory::GetForBrowserState(browser_state_)
      ->RemoveObserver(observer);
}

scoped_ptr<SigninClient::CookieChangedSubscription>
SigninClientImpl::AddCookieChangedCallback(
    const GURL& url,
    const std::string& name,
    const net::CookieStore::CookieChangedCallback& callback) {
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      browser_state_->GetRequestContext();
  DCHECK(context_getter.get());
  scoped_ptr<SigninCookieChangedSubscription> subscription(
      new SigninCookieChangedSubscription(context_getter, url, name, callback));
  return std::move(subscription);
}

void SigninClientImpl::OnSignedIn(const std::string& account_id,
                                  const std::string& gaia_id,
                                  const std::string& username,
                                  const std::string& password) {
  ios::ChromeBrowserStateManager* browser_state_manager =
      GetApplicationContext()->GetChromeBrowserStateManager();
  BrowserStateInfoCache* cache =
      browser_state_manager->GetBrowserStateInfoCache();
  size_t index = cache->GetIndexOfBrowserStateWithPath(
      browser_state_->GetOriginalChromeBrowserState()->GetStatePath());
  if (index != std::string::npos) {
    cache->SetAuthInfoOfBrowserStateAtIndex(index, gaia_id,
                                            base::UTF8ToUTF16(username));
  }
}

void SigninClientImpl::OnErrorChanged() {
  BrowserStateInfoCache* cache = GetApplicationContext()
                                     ->GetChromeBrowserStateManager()
                                     ->GetBrowserStateInfoCache();
  size_t index = cache->GetIndexOfBrowserStateWithPath(
      browser_state_->GetOriginalChromeBrowserState()->GetStatePath());
  if (index == std::string::npos)
    return;

  cache->SetBrowserStateIsAuthErrorAtIndex(
      index, signin_error_controller_->HasError());
}

void SigninClientImpl::OnGetTokenInfoResponse(
    scoped_ptr<base::DictionaryValue> token_info) {
  oauth_request_.reset();
}

void SigninClientImpl::OnOAuthError() {
  // Ignore the failure.  It's not essential and we'll try again next time.
  oauth_request_.reset();
}

void SigninClientImpl::OnNetworkError(int response_code) {
  // Ignore the failure.  It's not essential and we'll try again next time.
  oauth_request_.reset();
}

void SigninClientImpl::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  // Exchange the access token for a handle that can be used for later
  // verification that the token is still valid (i.e. the password has not
  // been changed).
  if (!oauth_client_) {
    oauth_client_.reset(
        new gaia::GaiaOAuthClient(browser_state_->GetRequestContext()));
  }
  oauth_client_->GetTokenInfo(access_token, 3 /* retries */, this);
}

void SigninClientImpl::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  // Ignore the failure.  It's not essential and we'll try again next time.
  oauth_request_.reset();
}

void SigninClientImpl::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type >= net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE)
    return;

  for (const base::Closure& callback : delayed_callbacks_)
    callback.Run();

  delayed_callbacks_.clear();
}

void SigninClientImpl::DelayNetworkCall(const base::Closure& callback) {
  // Don't bother if we don't have any kind of network connection.
  if (net::NetworkChangeNotifier::IsOffline()) {
    delayed_callbacks_.push_back(callback);
  } else {
    callback.Run();
  }
}

GaiaAuthFetcher* SigninClientImpl::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    const std::string& source,
    net::URLRequestContextGetter* getter) {
  return new GaiaAuthFetcherIOS(consumer, source, getter, browser_state_);
}
