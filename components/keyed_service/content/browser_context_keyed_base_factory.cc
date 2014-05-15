// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/browser_context_keyed_base_factory.h"

#include "base/prefs/pref_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

BrowserContextKeyedBaseFactory::BrowserContextKeyedBaseFactory(
    const char* name,
    BrowserContextDependencyManager* manager)
    : dependency_manager_(manager)
#ifndef NDEBUG
      ,
      service_name_(name)
#endif
{
  dependency_manager_->AddComponent(this);
}

BrowserContextKeyedBaseFactory::~BrowserContextKeyedBaseFactory() {
  dependency_manager_->RemoveComponent(this);
}

void BrowserContextKeyedBaseFactory::DependsOn(
    BrowserContextKeyedBaseFactory* rhs) {
  dependency_manager_->AddEdge(rhs, this);
}

void BrowserContextKeyedBaseFactory::RegisterProfilePrefsIfNecessaryForContext(
    const content::BrowserContext* context,
    user_prefs::PrefRegistrySyncable* registry) {
  std::set<const content::BrowserContext*>::iterator it =
      registered_preferences_.find(context);
  if (it == registered_preferences_.end()) {
    RegisterProfilePrefs(registry);
    registered_preferences_.insert(context);
  }
}

content::BrowserContext* BrowserContextKeyedBaseFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  DCHECK(CalledOnValidThread());

#ifndef NDEBUG
  dependency_manager_->AssertBrowserContextWasntDestroyed(context);
#endif

  // Safe default for the Incognito mode: no service.
  if (context->IsOffTheRecord())
    return NULL;

  return context;
}

void BrowserContextKeyedBaseFactory::RegisterUserPrefsOnBrowserContextForTest(
    content::BrowserContext* context) {
  // Safe timing for pref registration is hard. Previously, we made
  // BrowserContext responsible for all pref registration on every service
  // that used BrowserContext. Now we don't and there are timing issues.
  //
  // With normal contexts, prefs can simply be registered at
  // BrowserContextDependencyManager::RegisterProfilePrefsForServices time.
  // With incognito contexts, we just never register since incognito contexts
  // share the same pref services with their parent contexts.
  //
  // TestingBrowserContexts throw a wrench into the mix, in that some tests will
  // swap out the PrefService after we've registered user prefs on the original
  // PrefService. Test code that does this is responsible for either manually
  // invoking RegisterProfilePrefs() on the appropriate
  // BrowserContextKeyedServiceFactory associated with the prefs they need,
  // or they can use SetTestingFactory() and create a service (since service
  // creation with a factory method causes registration to happen at
  // TestingProfile creation time).
  //
  // Now that services are responsible for declaring their preferences, we have
  // to enforce a uniquenes check here because some tests create one context and
  // multiple services of the same type attached to that context (serially, not
  // parallel) and we don't want to register multiple times on the same context.
  // This is the purpose of RegisterProfilePrefsIfNecessary() which could be
  // replaced directly by RegisterProfilePrefs() if this method is ever phased
  // out.
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  user_prefs::PrefRegistrySyncable* registry =
      static_cast<user_prefs::PrefRegistrySyncable*>(
          prefs->DeprecatedGetPrefRegistry());
  RegisterProfilePrefsIfNecessaryForContext(context, registry);
}

bool BrowserContextKeyedBaseFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return false;
}

bool BrowserContextKeyedBaseFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

void BrowserContextKeyedBaseFactory::BrowserContextDestroyed(
    content::BrowserContext* context) {
  // While object destruction can be customized in ways where the object is
  // only dereferenced, this still must run on the UI thread.
  DCHECK(CalledOnValidThread());

  registered_preferences_.erase(context);
}

bool BrowserContextKeyedBaseFactory::ArePreferencesSetOn(
    content::BrowserContext* context) const {
  return registered_preferences_.find(context) != registered_preferences_.end();
}

void BrowserContextKeyedBaseFactory::MarkPreferencesSetOn(
    content::BrowserContext* context) {
  DCHECK(!ArePreferencesSetOn(context));
  registered_preferences_.insert(context);
}
