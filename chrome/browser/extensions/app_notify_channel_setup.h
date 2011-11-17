// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/app_notify_channel_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

class AppNotifyChannelSetupTest;
class Profile;

namespace content {
class NotificationDetails;
}

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
    : public content::URLFetcherDelegate,
      public content::NotificationObserver,
      public AppNotifyChannelUI::Delegate,
      public base::RefCountedThreadSafe<AppNotifyChannelSetup> {
 public:
  class Delegate {
   public:
    // If successful, |channel_id| will be non-empty. On failure, |channel_id|
    // will be empty and |error| will contain an error to report to the JS
    // callback.
    virtual void AppNotifyChannelSetupComplete(const std::string& channel_id,
                                               const std::string& error,
                                               int return_route_id,
                                               int callback_id) = 0;
  };

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

  // Implementing content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 protected:
  // content::URLFetcherDelegate.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

  // AppNotifyChannelUI::Delegate.
  virtual void OnSyncSetupResult(bool enabled) OVERRIDE;

 private:
  enum State {
    INITIAL,
    LOGIN_STARTED,
    LOGIN_DONE,
    FETCH_TOKEN_STARTED,
    FETCH_TOKEN_DONE,
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
  content::URLFetcher* CreateURLFetcher(
    const GURL& url, const std::string& body, const std::string& auth_token);
  void BeginLogin();
  void EndLogin(bool success);
  void BeginFetchTokens();
  void EndFetchTokens(bool success);
  void BeginRecordGrant();
  void EndRecordGrant(const content::URLFetcher* source);
  void BeginGetChannelId();
  void EndGetChannelId(const content::URLFetcher* source);

  void ReportResult(const std::string& channel_id, const std::string& error);

  static GURL GetCWSChannelServiceURL();
  static GURL GetOAuth2IssueTokenURL();
  static std::string MakeOAuth2IssueTokenBody(
      const std::string& oauth_client_id, const std::string& extension_id);
  static std::string MakeAuthorizationHeader(const std::string& auth_token);
  std::string GetLSOAuthToken();
  std::string GetCWSAuthToken();
  static bool ParseCWSChannelServiceResponse(
      const std::string& data, std::string* result);
  static bool IsGaiaServiceRelevant(const std::string& service);
  // Checks if the user needs to be prompted for login.
  bool ShouldPromptForLogin() const;
  // Checks if we need to fetch auth tokens for services we care about.
  virtual bool ShouldFetchServiceTokens() const;
  void RegisterForTokenServiceNotifications();
  void UnregisterForTokenServiceNotifications();

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  std::string extension_id_;
  std::string client_id_;
  GURL requestor_url_;
  int return_route_id_;
  int callback_id_;
  base::WeakPtr<Delegate> delegate_;
  scoped_ptr<content::URLFetcher> url_fetcher_;
  scoped_ptr<AppNotifyChannelUI> ui_;
  State state_;
  int fetch_token_success_count_;
  int fetch_token_fail_count_;

  DISALLOW_COPY_AND_ASSIGN(AppNotifyChannelSetup);
};

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_
