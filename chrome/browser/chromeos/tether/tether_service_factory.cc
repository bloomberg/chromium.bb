// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tether/tether_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/cryptauth/chrome_cryptauth_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

// static
TetherServiceFactory* TetherServiceFactory::GetInstance() {
  return base::Singleton<TetherServiceFactory>::get();
}

// static
TetherService* TetherServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<TetherService*>(
      TetherServiceFactory::GetInstance()->GetServiceForBrowserContext(
          browser_context, true));
}

TetherServiceFactory::TetherServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "TetherService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ChromeCryptAuthServiceFactory::GetInstance());
}

TetherServiceFactory::~TetherServiceFactory() {}

KeyedService* TetherServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(chromeos::NetworkHandler::IsInitialized());

  return new TetherService(
      Profile::FromBrowserContext(context),
      ChromeCryptAuthServiceFactory::GetForBrowserContext(
          Profile::FromBrowserContext(context)),
      chromeos::NetworkHandler::Get()->network_state_handler());
}

void TetherServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  TetherService::RegisterProfilePrefs(registry);
}
