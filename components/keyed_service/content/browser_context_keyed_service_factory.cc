// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#include <map>

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

void BrowserContextKeyedServiceFactory::SetTestingFactory(
    content::BrowserContext* context,
    TestingFactoryFunction testing_factory) {
  // Destroying the context may cause us to lose data about whether |context|
  // has our preferences registered on it (since the context object itself
  // isn't dead). See if we need to readd it once we've gone through normal
  // destruction.
  bool add_context = ArePreferencesSetOn(context);

#ifndef NDEBUG
  // Ensure that |context| is not marked as stale (e.g., due to it aliasing an
  // instance that was destroyed in an earlier test) in order to avoid accesses
  // to |context| in |BrowserContextShutdown| from causing
  // |AssertBrowserContextWasntDestroyed| to raise an error.
  dependency_manager_->MarkBrowserContextLiveForTesting(context);
#endif

  // We have to go through the shutdown and destroy mechanisms because there
  // are unit tests that create a service on a context and then change the
  // testing service mid-test.
  BrowserContextShutdown(context);
  BrowserContextDestroyed(context);

  if (add_context)
    MarkPreferencesSetOn(context);

  testing_factories_[context] = testing_factory;
}

KeyedService* BrowserContextKeyedServiceFactory::SetTestingFactoryAndUse(
    content::BrowserContext* context,
    TestingFactoryFunction testing_factory) {
  DCHECK(testing_factory);
  SetTestingFactory(context, testing_factory);
  return GetServiceForBrowserContext(context, true);
}

BrowserContextKeyedServiceFactory::BrowserContextKeyedServiceFactory(
    const char* name,
    BrowserContextDependencyManager* manager)
    : BrowserContextKeyedBaseFactory(name, manager) {}

BrowserContextKeyedServiceFactory::~BrowserContextKeyedServiceFactory() {
  DCHECK(mapping_.empty());
}

KeyedService* BrowserContextKeyedServiceFactory::GetServiceForBrowserContext(
    content::BrowserContext* context,
    bool create) {
  context = GetBrowserContextToUse(context);
  if (!context)
    return NULL;

  // NOTE: If you modify any of the logic below, make sure to update the
  // refcounted version in refcounted_context_keyed_service_factory.cc!
  BrowserContextKeyedServices::const_iterator it = mapping_.find(context);
  if (it != mapping_.end())
    return it->second;

  // Object not found.
  if (!create)
    return NULL;  // And we're forbidden from creating one.

  // Create new object.
  // Check to see if we have a per-BrowserContext testing factory that we should
  // use instead of default behavior.
  KeyedService* service = NULL;
  BrowserContextOverriddenTestingFunctions::const_iterator jt =
      testing_factories_.find(context);
  if (jt != testing_factories_.end()) {
    if (jt->second) {
      if (!context->IsOffTheRecord())
        RegisterUserPrefsOnBrowserContextForTest(context);
      service = jt->second(context);
    }
  } else {
    service = BuildServiceInstanceFor(context);
  }

  Associate(context, service);
  return service;
}

void BrowserContextKeyedServiceFactory::Associate(
    content::BrowserContext* context,
    KeyedService* service) {
  DCHECK(!ContainsKey(mapping_, context));
  mapping_.insert(std::make_pair(context, service));
}

void BrowserContextKeyedServiceFactory::Disassociate(
    content::BrowserContext* context) {
  BrowserContextKeyedServices::iterator it = mapping_.find(context);
  if (it != mapping_.end()) {
    delete it->second;
    mapping_.erase(it);
  }
}

void BrowserContextKeyedServiceFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  BrowserContextKeyedServices::iterator it = mapping_.find(context);
  if (it != mapping_.end() && it->second)
    it->second->Shutdown();
}

void BrowserContextKeyedServiceFactory::BrowserContextDestroyed(
    content::BrowserContext* context) {
  Disassociate(context);

  // For unit tests, we also remove the factory function both so we don't
  // maintain a big map of dead pointers, but also since we may have a second
  // object that lives at the same address (see other comments about unit tests
  // in this file).
  testing_factories_.erase(context);

  BrowserContextKeyedBaseFactory::BrowserContextDestroyed(context);
}

void BrowserContextKeyedServiceFactory::SetEmptyTestingFactory(
    content::BrowserContext* context) {
  SetTestingFactory(context, NULL);
}

bool BrowserContextKeyedServiceFactory::HasTestingFactory(
    content::BrowserContext* context) {
  return testing_factories_.find(context) != testing_factories_.end();
}

void BrowserContextKeyedServiceFactory::CreateServiceNow(
    content::BrowserContext* context) {
  GetServiceForBrowserContext(context, true);
}
