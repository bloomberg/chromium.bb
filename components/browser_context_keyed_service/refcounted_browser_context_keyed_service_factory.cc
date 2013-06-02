// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_context_keyed_service/refcounted_browser_context_keyed_service_factory.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/public/browser/browser_context.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/browser_context_keyed_service/refcounted_browser_context_keyed_service.h"

void RefcountedBrowserContextKeyedServiceFactory::SetTestingFactory(
    content::BrowserContext* context,
    FactoryFunction factory) {
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

  factories_[context] = factory;
}

scoped_refptr<RefcountedBrowserContextKeyedService>
RefcountedBrowserContextKeyedServiceFactory::SetTestingFactoryAndUse(
    content::BrowserContext* context,
    FactoryFunction factory) {
  DCHECK(factory);
  SetTestingFactory(context, factory);
  return GetServiceForBrowserContext(context, true);
}

RefcountedBrowserContextKeyedServiceFactory::
RefcountedBrowserContextKeyedServiceFactory(
    const char* name,
    BrowserContextDependencyManager* manager)
    : BrowserContextKeyedBaseFactory(name, manager) {
}

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
  BrowserContextOverriddenFunctions::const_iterator jt =
      factories_.find(context);
  if (jt != factories_.end()) {
    if (jt->second) {
      if (!context->IsOffTheRecord())
        RegisterUserPrefsOnBrowserContext(context);
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
  factories_.erase(context);

  BrowserContextKeyedBaseFactory::BrowserContextDestroyed(context);
}

void RefcountedBrowserContextKeyedServiceFactory::SetEmptyTestingFactory(
    content::BrowserContext* context) {
  SetTestingFactory(context, NULL);
}

void RefcountedBrowserContextKeyedServiceFactory::CreateServiceNow(
    content::BrowserContext* context) {
  GetServiceForBrowserContext(context, true);
}
