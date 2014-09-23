// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
#define EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
class NotificationSource;
}

namespace extensions {
class ContentRulesRegistry;
class ExtensionRegistry;
class RulesRegistryStorageDelegate;
}

namespace extensions {

// This class owns all RulesRegistries implementations of an ExtensionService.
// This class lives on the UI thread.
class RulesRegistryService : public BrowserContextKeyedAPI,
                             public content::NotificationObserver,
                             public ExtensionRegistryObserver {
 public:
  typedef RulesRegistry::WebViewKey WebViewKey;
  struct RulesRegistryKey {
    std::string event_name;
    WebViewKey webview_key;
    RulesRegistryKey(const std::string event_name,
                     const WebViewKey& webview_key)
        : event_name(event_name),
          webview_key(webview_key) {}
    bool operator<(const RulesRegistryKey& other) const {
      return (event_name < other.event_name) ||
          ((event_name == other.event_name) &&
          (webview_key < other.webview_key));
    }
  };

  explicit RulesRegistryService(content::BrowserContext* context);
  virtual ~RulesRegistryService();

  // Unregisters refptrs to concrete RulesRegistries at other objects that were
  // created by us so that the RulesRegistries can be released.
  virtual void Shutdown() OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<RulesRegistryService>*
      GetFactoryInstance();

  // Convenience method to get the RulesRegistryService for a context.
  static RulesRegistryService* Get(content::BrowserContext* context);

  // Registers the default RulesRegistries used in Chromium.
  void EnsureDefaultRulesRegistriesRegistered(const WebViewKey& webview_key);

  // Registers a RulesRegistry and wraps it in an InitializingRulesRegistry.
  void RegisterRulesRegistry(scoped_refptr<RulesRegistry> rule_registry);

  // Returns the RulesRegistry for |event_name| and |webview_key| or NULL if no
  // such registry has been registered. Default rules registries (such as the
  // WebRequest rules registry) will be created on first access.
  scoped_refptr<RulesRegistry> GetRulesRegistry(const WebViewKey& webview_key,
                                                const std::string& event_name);

  // Accessors for each type of rules registry.
  ContentRulesRegistry* content_rules_registry() const {
    CHECK(content_rules_registry_);
    return content_rules_registry_;
  }

  // Removes all rules registries of a given webview embedder process ID.
  void RemoveWebViewRulesRegistries(int process_id);

  // For testing.
  void SimulateExtensionUninstalled(const std::string& extension_id);

 private:
  friend class BrowserContextKeyedAPIFactory<RulesRegistryService>;

  // Maps <event name, webview key> to RuleRegistries that handle these
  // events.
  typedef std::map<RulesRegistryKey, scoped_refptr<RulesRegistry> >
      RulesRegistryMap;

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  // Iterates over all registries, and calls |notification_callback| on them
  // with |extension_id| as the argument. If a registry lives on a different
  // thread, the call is posted to that thread, so no guarantee of synchronous
  // processing.
  void NotifyRegistriesHelper(
      void (RulesRegistry::*notification_callback)(const std::string&),
      const std::string& extension_id);

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "RulesRegistryService";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  RulesRegistryMap rule_registries_;

  // We own the parts of the registries which need to run on the UI thread.
  ScopedVector<RulesCacheDelegate> cache_delegates_;

  // Weak pointer into rule_registries_ to make it easier to handle content rule
  // conditions.
  ContentRulesRegistry* content_rules_registry_;

  content::NotificationRegistrar registrar_;

  // Listen to extension load, unloaded notification.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(RulesRegistryService);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
