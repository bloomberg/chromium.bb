// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_API_H__

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/push_messaging/obfuscated_gaia_id_fetcher.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler_delegate.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/google_service_auth_error.h"

class Profile;

namespace extensions {

class PushMessagingInvalidationMapper;
class ObfuscatedGaiaIdFetcher;

// Observes a single InvalidationHandler and generates onMessage events.
class PushMessagingEventRouter
    : public PushMessagingInvalidationHandlerDelegate,
      public content::NotificationObserver {
 public:
  explicit PushMessagingEventRouter(Profile* profile);
  virtual ~PushMessagingEventRouter();

  PushMessagingInvalidationMapper* GetMapperForTest() const {
    return handler_.get();
  }
  void SetMapperForTest(scoped_ptr<PushMessagingInvalidationMapper> mapper);
  void TriggerMessageForTest(const std::string& extension_id,
                             int subchannel,
                             const std::string& payload);

 private:
  // InvalidationHandlerDelegate implementation.
  virtual void OnMessage(const std::string& extension_id,
                         int subchannel,
                         const std::string& payload) OVERRIDE;

  // content::NotificationDelegate implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
  Profile* const profile_;
  scoped_ptr<PushMessagingInvalidationMapper> handler_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingEventRouter);
};

class PushMessagingGetChannelIdFunction
    : public AsyncExtensionFunction,
      public ObfuscatedGaiaIdFetcher::Delegate,
      public LoginUIService::Observer {
 public:
  PushMessagingGetChannelIdFunction();

 protected:
  virtual ~PushMessagingGetChannelIdFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pushMessaging.getChannelId");

 private:
  void ReportResult(const std::string& gaia_id,
                    const std::string& error_message);

  void BuildAndSendResult(const std::string& gaia_id,
                          const std::string& error_message);

  // Begin the async fetch of the Gaia ID.
  bool StartGaiaIdFetch();

  // LoginUIService::Observer implementation.
  virtual void OnLoginUIShown(LoginUIService::LoginUI* ui) OVERRIDE;
  virtual void OnLoginUIClosed(LoginUIService::LoginUI* ui) OVERRIDE;

  // Check if the user is signed into chrome.
  bool IsUserLoggedIn() const;

  // ObfuscatedGiaiaIdFetcher::Delegate implementation.
  virtual void OnObfuscatedGaiaIdFetchSuccess(const std::string& gaia_id)
      OVERRIDE;
  virtual void OnObfuscatedGaiaIdFetchFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  scoped_ptr<ObfuscatedGaiaIdFetcher> fetcher_;
  bool interactive_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingGetChannelIdFunction);
};

class PushMessagingAPI : public ProfileKeyedService {
 public:
  explicit PushMessagingAPI(Profile* profile);
  virtual ~PushMessagingAPI();

  // Convenience method to get the PushMessagingAPI for a profile.
  static PushMessagingAPI* Get(Profile* profile);

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // For testing purposes.
  PushMessagingEventRouter* GetEventRouterForTest();

 private:
  // Created at ExtensionService startup.
  scoped_ptr<PushMessagingEventRouter> push_messaging_event_router_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingAPI);
};

}  // namespace extension

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_API_H__
