// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"

#include "base/logging.h"
#include "chrome/browser/extensions/api/declarative/initializing_rules_registry.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

RulesRegistryService::RulesRegistryService(Profile* profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

RulesRegistryService::~RulesRegistryService() {}

void RulesRegistryService::RegisterRulesRegistry(
    const std::string& event_name,
    scoped_ptr<RulesRegistry> rule_registry) {
  DCHECK(rule_registries_.find(event_name) == rule_registries_.end());
  rule_registries_[event_name] =
      make_linked_ptr(new InitializingRulesRegistry(rule_registry.Pass()));
}

RulesRegistry* RulesRegistryService::GetRulesRegistry(
    const std::string& event_name) const {
  RulesRegistryMap::const_iterator i = rule_registries_.find(event_name);
  if (i == rule_registries_.end())
    return NULL;
  return i->second.get();
}

void RulesRegistryService::OnExtensionUnloaded(
    const std::string& extension_id) {
  RulesRegistryMap::iterator i;
  for (i = rule_registries_.begin(); i != rule_registries_.end(); ++i)
    i->second->OnExtensionUnloaded(extension_id);
}

void RulesRegistryService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      OnExtensionUnloaded(extension->id());
      break;
    }
  default:
    NOTREACHED();
  }
}

}  // namespace extensions
