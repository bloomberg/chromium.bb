// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
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
class RulesRegistryService : public content::NotificationObserver  {
 public:
  explicit RulesRegistryService(Profile* profile);
  virtual ~RulesRegistryService();

  // Registers a RulesRegistry and wraps it in an InitializingRulesRegistry.
  void RegisterRulesRegistry(const std::string& event_name,
                             scoped_ptr<RulesRegistry> rule_registry);

  // Returns the RulesRegistry for |event_name| or NULL if no such registry
  // has been registered.
  RulesRegistry* GetRulesRegistry(const std::string& event_name) const;

 private:
  // Maps event names to RuleRegistries that handle these events.
  typedef std::map<std::string, linked_ptr<RulesRegistry> > RulesRegistryMap;

  // Notifies all RulesRegistries that |extension_id| was unloaded.
  void OnExtensionUnloaded(const std::string& extension_id);

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  RulesRegistryMap rule_registries_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RulesRegistryService);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_RULES_REGISTRY_SERVICE_H__
