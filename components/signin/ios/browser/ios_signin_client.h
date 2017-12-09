// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_IOS_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_IOS_SIGNIN_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "net/url_request/url_request_context_getter.h"
#include "components/signin/ios/browser/wait_for_network_callback_helper.h"

// iOS specific signin client.
class IOSSigninClient
    : public SigninClient,
      public SigninErrorController::Observer {
 public:
  IOSSigninClient(
      PrefService* pref_service,
      net::URLRequestContextGetter* url_request_context,
      SigninErrorController* signin_error_controller,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      scoped_refptr<HostContentSettingsMap> host_content_settings_map,
      scoped_refptr<TokenWebData> token_web_data);
  ~IOSSigninClient() override;

  // SigninClient implementation.
  scoped_refptr<TokenWebData> GetDatabase() override;
  PrefService* GetPrefs() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  void DoFinalInit() override;
  bool CanRevokeCredentials() override;
  std::string GetSigninScopedDeviceId() override;
  bool ShouldMergeSigninCredentialsIntoCookieJar() override;
  bool IsFirstRun() const override;
  bool AreSigninCookiesAllowed() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  std::unique_ptr<CookieChangedSubscription> AddCookieChangedCallback(
      const GURL& url,
      const std::string& name,
      const net::CookieStore::CookieChangedCallback& callback) override;
  void DelayNetworkCall(const base::Closure& callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      net::URLRequestContextGetter* getter) override;

  // KeyedService implementation.
  void Shutdown() override;

 private:
  PrefService* pref_service_;
  net::URLRequestContextGetter* url_request_context_;
  SigninErrorController* signin_error_controller_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<TokenWebData> token_web_data_;
  std::unique_ptr<WaitForNetworkCallbackHelper> callback_helper_;

  DISALLOW_COPY_AND_ASSIGN(IOSSigninClient);
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_IOS_SIGNIN_CLIENT_H_
