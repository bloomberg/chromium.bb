// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/app_notify_channel_ui.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;

namespace extensions {
class AppNotifyChannelSetupTest;

// This class uses the browser login credentials to setup app notifications
// for a given app.
//
// Performs the following steps when Start() is called:
// 1. If the user is not logged in, prompt the user to login.
// 2. OAuth2: Record a notifications grant for the user and the given app.
// 3. Get notifications channel id for the current user.
// 4. Call the delegate passed in to the constructor with the results of
//    the above steps.
class AppNotifyChannelSetup
    : public net::URLFetcherDelegate,
      public AppNotifyChannelUI::Delegate,
      public OAuth2AccessTokenConsumer,
      public base::RefCountedThreadSafe<AppNotifyChannelSetup> {
 public:
  // These are the various error conditions, made public for use by the test
  // interceptor.
  enum SetupError {
    NONE,
    AUTH_ERROR,
    INTERNAL_ERROR,
    USER_CANCELLED,

    // This is used for histograms, and should always be the last value.
    SETUP_ERROR_BOUNDARY
  };

  class Delegate {
   public:
    // If successful, |channel_id| will be non-empty. On failure, |channel_id|
    // will be empty and |error| will contain an error to report to the JS
    // callback.
    virtual void AppNotifyChannelSetupComplete(
        const std::string& channel_id,
        const std::string& error,
        const AppNotifyChannelSetup* setup) = 0;
  };

  // For tests, we allow intercepting the request to setup the channel and
  // forcing the return of a certain result to the delegate.
  class InterceptorForTests {
   public:
    virtual void DoIntercept(
        const AppNotifyChannelSetup* setup,
        std::string* result_channel_id,
        AppNotifyChannelSetup::SetupError* result_error) = 0;
  };
  static void SetInterceptorForTests(InterceptorForTests* interceptor);

  // Ownership of |ui| is transferred to this object.
  AppNotifyChannelSetup(Profile* profile,
                        const std::string& extension_id,
                        const std::string& client_id,
                        const GURL& requestor_url,
                        int return_route_id,
                        int callback_id,
                        AppNotifyChannelUI* ui,
                        base::WeakPtr<Delegate> delegate);

  AppNotifyChannelUI* ui() { return ui_.get(); }

  // This begins the process of fetching the channel id using the browser login
  // credentials (or using |ui_| to prompt for login if needed).
  void Start();

  // OAuth2AccessTokenConsumer implementation.
  virtual void OnGetTokenSuccess(const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;


  // Getters for various members.
  const std::string& extension_id() const { return extension_id_; }
  const std::string& client_id() const { return client_id_; }
  int return_route_id() const { return return_route_id_; }
  int callback_id() const { return callback_id_; }

 protected:
  // net::URLFetcherDelegate.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // AppNotifyChannelUI::Delegate.
  virtual void OnSyncSetupResult(bool enabled) OVERRIDE;

 private:
  enum State {
    INITIAL,
    LOGIN_STARTED,
    LOGIN_DONE,
    FETCH_ACCESS_TOKEN_STARTED,
    FETCH_ACCESS_TOKEN_DONE,
    RECORD_GRANT_STARTED,
    RECORD_GRANT_DONE,
    CHANNEL_ID_SETUP_STARTED,
    CHANNEL_ID_SETUP_DONE,
    ERROR_STATE
  };

  friend class base::RefCountedThreadSafe<AppNotifyChannelSetup>;
  friend class AppNotifyChannelSetupTest;

  virtual ~AppNotifyChannelSetup();

  // Creates an instance of URLFetcher that does not send or save cookies.
  // The URLFether's method will be GET if body is empty, POST otherwise.
  // Caller owns the returned instance.
  net::URLFetcher* CreateURLFetcher(
    const GURL& url, const std::string& body, const std::string& auth_token);
  void BeginLogin();
  void EndLogin(bool success);
  void BeginGetAccessToken();
  void EndGetAccessToken(bool success);
  void BeginRecordGrant();
  void EndRecordGrant(const net::URLFetcher* source);
  void BeginGetChannelId();
  void EndGetChannelId(const net::URLFetcher* source);

  void ReportResult(const std::string& channel_id, SetupError error);

  static std::string GetErrorString(SetupError error);
  static GURL GetCWSChannelServiceURL();
  static GURL GetOAuth2IssueTokenURL();
  static std::string MakeOAuth2IssueTokenBody(
      const std::string& oauth_client_id, const std::string& extension_id);
  static std::string MakeAuthorizationHeader(const std::string& auth_token);
  static bool ParseCWSChannelServiceResponse(
      const std::string& data, std::string* result);
  // Checks if the user needs to be prompted for login.
  bool ShouldPromptForLogin() const;
  void RegisterForTokenServiceNotifications();
  void UnregisterForTokenServiceNotifications();

  Profile* profile_;
  std::string extension_id_;
  std::string client_id_;
  GURL requestor_url_;
  int return_route_id_;
  int callback_id_;
  base::WeakPtr<Delegate> delegate_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_ptr<OAuth2AccessTokenFetcher> oauth2_fetcher_;
  scoped_ptr<AppNotifyChannelUI> ui_;
  State state_;
  std::string oauth2_access_token_;
  // Keeps track of whether we have encountered failure in OAuth2 access
  // token generation already. We use this to prevent us from doing an
  // infinite loop of trying to generate access token, if that fails, try
  // to login the user and generate access token, etc.
  bool oauth2_access_token_failure_;

  DISALLOW_COPY_AND_ASSIGN(AppNotifyChannelSetup);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_
