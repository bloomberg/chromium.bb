// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_domain_descriptor.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/common/policy/policy_schema.h"

namespace policy {

namespace {

bool Matches(const PolicySchema* schema, const base::Value& value) {
  if (!schema) {
    // Schema not found, invalid entry.
    return false;
  }

  if (!value.IsType(schema->type()))
    return false;

  const base::DictionaryValue* dict = NULL;
  const base::ListValue* list = NULL;
  if (value.GetAsDictionary(&dict)) {
    for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd();
         it.Advance()) {
      if (!Matches(schema->GetSchemaForProperty(it.key()), it.value()))
        return false;
    }
  } else if (value.GetAsList(&list)) {
    for (base::ListValue::const_iterator it = list->begin();
         it != list->end(); ++it) {
      if (!*it || !Matches(schema->GetSchemaForItems(), **it))
        return false;
    }
  }

  return true;
}

}  // namespace

PolicyDomainDescriptor::PolicyDomainDescriptor(PolicyDomain domain)
    : domain_(domain) {}

void PolicyDomainDescriptor::RegisterComponent(
    const std::string& component_id,
    scoped_ptr<PolicySchema> schema) {
  const PolicySchema*& entry = schema_map_[component_id];
  delete entry;
  entry = schema.release();
}

void PolicyDomainDescriptor::FilterBundle(PolicyBundle* bundle) const {
  // Chrome policies are not filtered, so that typos appear in about:policy.
  DCHECK_NE(POLICY_DOMAIN_CHROME, domain_);

  for (PolicyBundle::iterator it_bundle = bundle->begin();
       it_bundle != bundle->end(); ++it_bundle) {
    const PolicyNamespace& ns = it_bundle->first;
    if (ns.domain != domain_)
      continue;

    SchemaMap::const_iterator it_schema = schema_map_.find(ns.component_id);
    if (it_schema == schema_map_.end()) {
      // Component ID not found.
      it_bundle->second->Clear();
      continue;
    }

    // TODO(joaodasilva): if a component is registered but doesn't have a schema
    // then its policies aren't filtered. This behavior is enabled for M29 to
    // allow a graceful update of the Legacy Browser Support extension; it'll
    // be removed for M30. http://crbug.com/240704
    if (!it_schema->second)
      continue;

    const PolicySchema* component_schema = it_schema->second;
    PolicyMap* map = it_bundle->second;
    for (PolicyMap::const_iterator it_map = map->begin();
         it_map != map->end();) {
      const std::string& policy_name = it_map->first;
      const base::Value* policy_value = it_map->second.value;
      const PolicySchema* policy_schema =
          component_schema->GetSchemaForProperty(policy_name);
      ++it_map;
      if (!policy_value || !Matches(policy_schema, *policy_value))
        map->Erase(policy_name);
    }
  }
}

PolicyDomainDescriptor::~PolicyDomainDescriptor() {
  STLDeleteValues(&schema_map_);
}

}  // namespace policy
