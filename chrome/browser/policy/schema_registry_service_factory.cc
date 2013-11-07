// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/schema_registry_service_factory.h"

#include "base/logging.h"
#include "chrome/browser/policy/schema_registry.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/policy/core/common/schema.h"
#include "content/public/browser/browser_context.h"

namespace policy {

// static
SchemaRegistryServiceFactory* SchemaRegistryServiceFactory::GetInstance() {
  return Singleton<SchemaRegistryServiceFactory>::get();
}

// static
SchemaRegistryService* SchemaRegistryServiceFactory::GetForContext(
    content::BrowserContext* context) {
  return GetInstance()->GetForContextInternal(context);
}

// static
scoped_ptr<SchemaRegistryService>
SchemaRegistryServiceFactory::CreateForContext(
    content::BrowserContext* context,
    const Schema& chrome_schema,
    CombinedSchemaRegistry* global_registry) {
  return GetInstance()->CreateForContextInternal(
      context, chrome_schema, global_registry);
}

SchemaRegistryServiceFactory::SchemaRegistryServiceFactory()
    : BrowserContextKeyedBaseFactory(
          "SchemaRegistryService",
          BrowserContextDependencyManager::GetInstance()) {}

SchemaRegistryServiceFactory::~SchemaRegistryServiceFactory() {}

SchemaRegistryService* SchemaRegistryServiceFactory::GetForContextInternal(
    content::BrowserContext* context) {
  // Off-the-record Profiles get their policy from the main Profile's
  // PolicyService, and don't need their own SchemaRegistry nor any policy
  // providers.
  if (context->IsOffTheRecord())
    return NULL;
  RegistryMap::const_iterator it = registries_.find(context);
  CHECK(it != registries_.end());
  return it->second;
}

scoped_ptr<SchemaRegistryService>
SchemaRegistryServiceFactory::CreateForContextInternal(
    content::BrowserContext* context,
    const Schema& chrome_schema,
    CombinedSchemaRegistry* global_registry) {
  DCHECK(!context->IsOffTheRecord());
  DCHECK(registries_.find(context) == registries_.end());
  SchemaRegistryService* registry =
      new SchemaRegistryService(chrome_schema, global_registry);
  registries_[context] = registry;
  return make_scoped_ptr(registry);
}

void SchemaRegistryServiceFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  if (context->IsOffTheRecord())
    return;
  RegistryMap::iterator it = registries_.find(context);
  if (it != registries_.end())
    it->second->Shutdown();
  else
    NOTREACHED();
}

void SchemaRegistryServiceFactory::BrowserContextDestroyed(
    content::BrowserContext* context) {
  registries_.erase(context);
  BrowserContextKeyedBaseFactory::BrowserContextDestroyed(context);
}

void SchemaRegistryServiceFactory::SetEmptyTestingFactory(
    content::BrowserContext* context) {}

void SchemaRegistryServiceFactory::CreateServiceNow(
    content::BrowserContext* context) {}

}  // namespace policy
