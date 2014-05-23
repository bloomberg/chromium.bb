// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"

using content::BrowserContext;

// static
HotwordService* HotwordServiceFactory::GetForProfile(BrowserContext* context) {
  return static_cast<HotwordService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
HotwordServiceFactory* HotwordServiceFactory::GetInstance() {
  return Singleton<HotwordServiceFactory>::get();
}

// static
bool HotwordServiceFactory::ShouldShowOptInPopup(BrowserContext* context) {
  HotwordService* hotword_service = GetForProfile(context);
  return hotword_service && hotword_service->ShouldShowOptInPopup();
}

// static
bool HotwordServiceFactory::IsServiceAvailable(BrowserContext* context) {
  HotwordService* hotword_service = GetForProfile(context);
  return hotword_service && hotword_service->IsServiceAvailable();
}

// static
bool HotwordServiceFactory::IsHotwordAllowed(BrowserContext* context) {
  HotwordService* hotword_service = GetForProfile(context);
  return hotword_service && hotword_service->IsHotwordAllowed();
}

// static
int HotwordServiceFactory::GetCurrentError(BrowserContext* context) {
  HotwordService* hotword_service = GetForProfile(context);
  if (!hotword_service)
    return 0;
  return hotword_service->error_message();
}

HotwordServiceFactory::HotwordServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "HotwordService",
        BrowserContextDependencyManager::GetInstance()) {
  // No dependencies.
}

HotwordServiceFactory::~HotwordServiceFactory() {
}

void HotwordServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kHotwordSearchEnabled,
                             false,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kHotwordOptInPopupTimesShown,
                             0,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  // Although this is default true, users will not send back information to
  // Google unless they have opted in to Hotwording at which point they must
  // also confirm that they wish this preference to be true or opt out of it.
  prefs->RegisterBooleanPref(prefs::kHotwordAudioLoggingEnabled,
                             true,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

KeyedService* HotwordServiceFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new HotwordService(Profile::FromBrowserContext(context));
}
