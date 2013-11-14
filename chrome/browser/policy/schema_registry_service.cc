// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/schema_registry_service.h"

#include "base/logging.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"

namespace policy {

SchemaRegistryService::SchemaRegistryService(
    const Schema& chrome_schema,
    CombinedSchemaRegistry* global_registry)
    : global_registry_(global_registry) {
  if (chrome_schema.valid())
    RegisterComponent(PolicyNamespace(POLICY_DOMAIN_CHROME, ""), chrome_schema);
  SetReady(POLICY_DOMAIN_CHROME);
  if (global_registry_)
    global_registry->Track(this);
}

SchemaRegistryService::~SchemaRegistryService() {}

void SchemaRegistryService::Shutdown() {
  if (global_registry_) {
    global_registry_->Untrack(this);
    global_registry_ = NULL;
  }
  DCHECK(!HasObservers());
}

}  // namespace policy
