// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

void RefcountedBrowserContextKeyedServiceFactory::SetTestingFactory(
    content::BrowserContext* context,
    TestingFactoryFunction testing_factory) {
  // Destroying the context may cause us to lose data about whether |context|
  // has our preferences registered on it (since the context object itself
  // isn't dead). See if we need to readd it once we've gone through normal
  // destruction.
  bool add_context = ArePreferencesSetOn(context);

  // We have to go through the shutdown and destroy mechanisms because there
  // are unit tests that create a service on a context and then change the
  // testing service mid-test.
  BrowserContextShutdown(context);
  BrowserContextDestroyed(context);

  if (add_context)
    MarkPreferencesSetOn(context);

  testing_factories_[context] = testing_factory;
}

scoped_refptr<RefcountedBrowserContextKeyedService>
RefcountedBrowserContextKeyedServiceFactory::SetTestingFactoryAndUse(
    content::BrowserContext* context,
    TestingFactoryFunction testing_factory) {
  DCHECK(testing_factory);
  SetTestingFactory(context, testing_factory);
  return GetServiceForBrowserContext(context, true);
}

RefcountedBrowserContextKeyedServiceFactory::
    RefcountedBrowserContextKeyedServiceFactory(
        const char* name,
        BrowserContextDependencyManager* manager)
    : BrowserContextKeyedBaseFactory(name, manager) {}

RefcountedBrowserContextKeyedServiceFactory::
    ~RefcountedBrowserContextKeyedServiceFactory() {
  DCHECK(mapping_.empty());
}

scoped_refptr<RefcountedBrowserContextKeyedService>
RefcountedBrowserContextKeyedServiceFactory::GetServiceForBrowserContext(
    content::BrowserContext* context,
    bool create) {
  context = GetBrowserContextToUse(context);
  if (!context)
    return NULL;

  // NOTE: If you modify any of the logic below, make sure to update the
  // non-refcounted version in context_keyed_service_factory.cc!
  RefCountedStorage::const_iterator it = mapping_.find(context);
  if (it != mapping_.end())
    return it->second;

  // Object not found.
  if (!create)
    return NULL;  // And we're forbidden from creating one.

  // Create new object.
  // Check to see if we have a per-BrowserContext testing factory that we should
  // use instead of default behavior.
  scoped_refptr<RefcountedBrowserContextKeyedService> service;
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

void RefcountedBrowserContextKeyedServiceFactory::Associate(
    content::BrowserContext* context,
    const scoped_refptr<RefcountedBrowserContextKeyedService>& service) {
  DCHECK(!ContainsKey(mapping_, context));
  mapping_.insert(std::make_pair(context, service));
}

void RefcountedBrowserContextKeyedServiceFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  RefCountedStorage::iterator it = mapping_.find(context);
  if (it != mapping_.end() && it->second.get())
    it->second->ShutdownOnUIThread();
}

void RefcountedBrowserContextKeyedServiceFactory::BrowserContextDestroyed(
    content::BrowserContext* context) {
  // We "merely" drop our reference to the service. Hopefully this will cause
  // the service to be destroyed. If not, oh well.
  mapping_.erase(context);

  // For unit tests, we also remove the factory function both so we don't
  // maintain a big map of dead pointers, but also since we may have a second
  // object that lives at the same address (see other comments about unit tests
  // in this file).
  testing_factories_.erase(context);

  BrowserContextKeyedBaseFactory::BrowserContextDestroyed(context);
}

void RefcountedBrowserContextKeyedServiceFactory::SetEmptyTestingFactory(
    content::BrowserContext* context) {
  SetTestingFactory(context, NULL);
}

bool RefcountedBrowserContextKeyedServiceFactory::HasTestingFactory(
    content::BrowserContext* context) {
  return testing_factories_.find(context) != testing_factories_.end();
}

void RefcountedBrowserContextKeyedServiceFactory::CreateServiceNow(
    content::BrowserContext* context) {
  GetServiceForBrowserContext(context, true);
}
