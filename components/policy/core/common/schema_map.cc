// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema_map.h"

#include "base/logging.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"

namespace policy {

SchemaMap::SchemaMap() {}

SchemaMap::SchemaMap(DomainMap& map) {
  map_.swap(map);
}

SchemaMap::~SchemaMap() {}

const DomainMap& SchemaMap::GetDomains() const {
  return map_;
}

const ComponentMap* SchemaMap::GetComponents(PolicyDomain domain) const {
  DomainMap::const_iterator it = map_.find(domain);
  return it == map_.end() ? NULL : &it->second;
}

const Schema* SchemaMap::GetSchema(const PolicyNamespace& ns) const {
  const ComponentMap* map = GetComponents(ns.domain);
  if (!map)
    return NULL;
  ComponentMap::const_iterator it = map->find(ns.component_id);
  return it == map->end() ? NULL : &it->second;
}

void SchemaMap::FilterBundle(PolicyBundle* bundle) const {
  for (PolicyBundle::iterator it = bundle->begin(); it != bundle->end(); ++it) {
    // Chrome policies are not filtered, so that typos appear in about:policy.
    // Everything else gets filtered, so that components only see valid policy.
    if (it->first.domain == POLICY_DOMAIN_CHROME)
      continue;

    const Schema* schema = GetSchema(it->first);

    if (!schema) {
      it->second->Clear();
      continue;
    }

    // TODO(joaodasilva): if a component is registered but doesn't have a schema
    // then its policies aren't filtered. This behavior is enabled to allow a
    // graceful update of the Legacy Browser Support extension; it'll be removed
    // in a future release. http://crbug.com/240704
    static const char kLegacyBrowserSupportExtensionId[] =
        "heildphpnddilhkemkielfhnkaagiabh";
    if (it->first.domain == POLICY_DOMAIN_EXTENSIONS &&
        it->first.component_id == kLegacyBrowserSupportExtensionId) {
      continue;
    }

    if (!schema->valid()) {
      // Don't serve unknown policies.
      it->second->Clear();
      continue;
    }

    PolicyMap* map = it->second;
    for (PolicyMap::const_iterator it_map = map->begin();
         it_map != map->end();) {
      const std::string& policy_name = it_map->first;
      const base::Value* policy_value = it_map->second.value;
      Schema policy_schema = schema->GetProperty(policy_name);
      ++it_map;
      std::string error_path;
      std::string error;
      if (!policy_value ||
          !policy_schema.Validate(*policy_value,
                                  SCHEMA_STRICT,
                                  &error_path,
                                  &error)) {
        LOG(ERROR) << "Dropping policy " << policy_name << " for "
                   << it->first.component_id
                   << " because it's not valid: " << error
                   << " at " << error_path;
        map->Erase(policy_name);
      }
    }
  }
}

bool SchemaMap::HasComponents() const {
  for (DomainMap::const_iterator domain = map_.begin();
       domain != map_.end(); ++domain) {
    if (domain->first == POLICY_DOMAIN_CHROME)
      continue;
    if (!domain->second.empty())
      return true;
  }
  return false;
}

void SchemaMap::GetChanges(const scoped_refptr<SchemaMap>& older,
                           PolicyNamespaceList* removed,
                           PolicyNamespaceList* added) const {
  GetNamespacesNotInOther(older, added);
  older->GetNamespacesNotInOther(this, removed);
}

void SchemaMap::GetNamespacesNotInOther(const SchemaMap* other,
                                        PolicyNamespaceList* list) const {
  list->clear();
  for (DomainMap::const_iterator domain = map_.begin();
       domain != map_.end(); ++domain) {
    const ComponentMap& components = domain->second;
    for (ComponentMap::const_iterator comp = components.begin();
         comp != components.end(); ++comp) {
      PolicyNamespace ns(domain->first, comp->first);
      if (!other->GetSchema(ns))
        list->push_back(ns);
    }
  }
}

}  // namespace policy
