// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/automatic_profile_resetter_factory.h"

#include "base/memory/singleton.h"
#include "base/prefs/pref_registry_simple.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"

// static
AutomaticProfileResetter* AutomaticProfileResetterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AutomaticProfileResetter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AutomaticProfileResetterFactory*
AutomaticProfileResetterFactory::GetInstance() {
  return Singleton<AutomaticProfileResetterFactory>::get();
}

// static
void AutomaticProfileResetterFactory::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProfileResetPromptMemento);
}

AutomaticProfileResetterFactory::AutomaticProfileResetterFactory()
    : BrowserContextKeyedServiceFactory(
          "AutomaticProfileResetter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

AutomaticProfileResetterFactory::~AutomaticProfileResetterFactory() {}

BrowserContextKeyedService*
AutomaticProfileResetterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  AutomaticProfileResetter* service = new AutomaticProfileResetter(profile);
  service->Activate();
  return service;
}

void AutomaticProfileResetterFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      prefs::kProfileResetPromptMemento,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool AutomaticProfileResetterFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AutomaticProfileResetterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
