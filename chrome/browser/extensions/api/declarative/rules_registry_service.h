// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/extensions/api/declarative/rules_registry.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace content {
class NotificationSource;
class NotificationSource;
}

namespace extensions {
class ContentRulesRegistry;
class RulesRegistry;
class RulesRegistryStorageDelegate;
}

namespace extensions {

// This class owns all RulesRegistries implementations of an ExtensionService.
// This class lives on the UI thread.
class RulesRegistryService : public ProfileKeyedAPI,
                             public content::NotificationObserver {
 public:
  explicit RulesRegistryService(Profile* profile);
  virtual ~RulesRegistryService();

  // Unregisters refptrs to concrete RulesRegistries at other objects that were
  // created by us so that the RulesRegistries can be released.
  virtual void Shutdown() OVERRIDE;

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<RulesRegistryService>* GetFactoryInstance();

  // Convenience method to get the RulesRegistryService for a profile.
  static RulesRegistryService* Get(Profile* profile);

  // Registers the default RulesRegistries used in Chromium.
  void RegisterDefaultRulesRegistries();

  // Registers a RulesRegistry and wraps it in an InitializingRulesRegistry.
  void RegisterRulesRegistry(scoped_refptr<RulesRegistry> rule_registry);

  // Returns the RulesRegistry for |event_name| or NULL if no such registry
  // has been registered.
  scoped_refptr<RulesRegistry> GetRulesRegistry(
      const std::string& event_name) const;

  // Accessors for each type of rules registry.
  ContentRulesRegistry* content_rules_registry() const {
    return content_rules_registry_;
  }

  // For testing.
  void SimulateExtensionUnloaded(const std::string& extension_id);
 private:
  friend class ProfileKeyedAPIFactory<RulesRegistryService>;

  // Maps event names to RuleRegistries that handle these events.
  typedef std::map<std::string, scoped_refptr<RulesRegistry> >
      RulesRegistryMap;

  // Notifies all RulesRegistries that |extension_id| was unloaded.
  // It is not guaranteed that this notification is processed synchronously.
  // If extensions live on another thread, the notification is posted.
  void OnExtensionUnloaded(const std::string& extension_id);

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "RulesRegistryService";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  RulesRegistryMap rule_registries_;

  // We own the parts of the registries which need to run on the UI thread.
  ScopedVector<RulesCacheDelegate> ui_parts_of_registries_;

  // Weak pointer into rule_registries_ to make it easier to handle content rule
  // conditions.
  ContentRulesRegistry* content_rules_registry_;

  content::NotificationRegistrar registrar_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(RulesRegistryService);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
