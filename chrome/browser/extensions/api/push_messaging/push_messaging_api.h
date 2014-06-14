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
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/push_messaging/obfuscated_gaia_id_fetcher.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler_delegate.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionRegistry;
class PushMessagingInvalidationMapper;

// Observes a single InvalidationHandler and generates onMessage events.
class PushMessagingEventRouter
    : public PushMessagingInvalidationHandlerDelegate {
 public:
  explicit PushMessagingEventRouter(content::BrowserContext* context);
  virtual ~PushMessagingEventRouter();

  // For testing purposes.
  void TriggerMessageForTest(const std::string& extension_id,
                             int subchannel,
                             const std::string& payload);

 private:
  // InvalidationHandlerDelegate implementation.
  virtual void OnMessage(const std::string& extension_id,
                         int subchannel,
                         const std::string& payload) OVERRIDE;

  content::BrowserContext* const browser_context_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingEventRouter);
};

class PushMessagingGetChannelIdFunction
    : public ChromeAsyncExtensionFunction,
      public ObfuscatedGaiaIdFetcher::Delegate,
      public OAuth2TokenService::Observer,
      public OAuth2TokenService::Consumer {
 public:
  PushMessagingGetChannelIdFunction();

 protected:
  virtual ~PushMessagingGetChannelIdFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("pushMessaging.getChannelId",
                             PUSHMESSAGING_GETCHANNELID)

 private:
  void ReportResult(const std::string& gaia_id,
                    const std::string& error_message);

  void BuildAndSendResult(const std::string& gaia_id,
                          const std::string& error_message);

  // Begin the async fetch of the Gaia ID.
  void StartGaiaIdFetch(const std::string& access_token);

  // Begin the async fetch of the access token for Gaia ID fetcher.
  void StartAccessTokenFetch();

  // OAuth2TokenService::Observer implementation.
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;

  // OAuth2TokenService::Consumer implementation.
  virtual void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) OVERRIDE;

  // ObfuscatedGiaiaIdFetcher::Delegate implementation.
  virtual void OnObfuscatedGaiaIdFetchSuccess(const std::string& gaia_id)
      OVERRIDE;
  virtual void OnObfuscatedGaiaIdFetchFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  scoped_ptr<ObfuscatedGaiaIdFetcher> fetcher_;
  bool interactive_;
  scoped_ptr<OAuth2TokenService::Request> fetcher_access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingGetChannelIdFunction);
};

class PushMessagingAPI : public BrowserContextKeyedAPI,
                         public ExtensionRegistryObserver {
 public:
  explicit PushMessagingAPI(content::BrowserContext* context);
  virtual ~PushMessagingAPI();

  // Convenience method to get the PushMessagingAPI for a BrowserContext.
  static PushMessagingAPI* Get(content::BrowserContext* context);

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PushMessagingAPI>* GetFactoryInstance();

  // For testing purposes.
  PushMessagingEventRouter* GetEventRouterForTest() const {
    return event_router_.get();
  }
  PushMessagingInvalidationMapper* GetMapperForTest() const {
    return handler_.get();
  }
  void SetMapperForTest(scoped_ptr<PushMessagingInvalidationMapper> mapper);

 private:
  friend class BrowserContextKeyedAPIFactory<PushMessagingAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "PushMessagingAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Overridden from ExtensionRegistryObserver.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) OVERRIDE;

  // Initialize |event_router_| and |handler_|.
  bool InitEventRouterAndHandler();

  // Created lazily when an app or extension with the push messaging permission
  // is loaded.
  scoped_ptr<PushMessagingEventRouter> event_router_;
  scoped_ptr<PushMessagingInvalidationMapper> handler_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingAPI);
};

template <>
void BrowserContextKeyedAPIFactory<
    PushMessagingAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_PUSH_MESSAGING_API_H__
