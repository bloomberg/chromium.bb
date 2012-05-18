// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace content {
class NotificationSource;
class NotificationSource;
}

namespace extensions {
class RulesRegistry;
}

namespace extensions {

// This class owns all RulesRegistries implementations of an ExtensionService.
// This class lives on the UI thread.
class RulesRegistryService : public content::NotificationObserver  {
 public:
  explicit RulesRegistryService(Profile* profile);
  virtual ~RulesRegistryService();

  // Unregisters refptrs to concrete RulesRegistries at other objects that were
  // created by us so that the RulesRegistries can be released.
  void Shutdown();

  // Registers the default RulesRegistries used in Chromium.
  void RegisterDefaultRulesRegistries();

  // Registers a RulesRegistry and wraps it in an InitializingRulesRegistry.
  void RegisterRulesRegistry(const std::string& event_name,
                             scoped_refptr<RulesRegistry> rule_registry);

  // Returns the RulesRegistry for |event_name| or NULL if no such registry
  // has been registered.
  scoped_refptr<RulesRegistry> GetRulesRegistry(
      const std::string& event_name) const;

  // For testing.
  void SimulateExtensionUnloaded(const std::string& extension_id);
 private:
  // Maps event names to RuleRegistries that handle these events.
  typedef std::map<std::string, scoped_refptr<RulesRegistry> > RulesRegistryMap;

  // Notifies all RulesRegistries that |extension_id| was unloaded.
  // It is not guaranteed that this notification is processed synchronously.
  // If extensions live on another thread, the notification is posted.
  void OnExtensionUnloaded(const std::string& extension_id);

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  RulesRegistryMap rule_registries_;

  content::NotificationRegistrar registrar_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(RulesRegistryService);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
