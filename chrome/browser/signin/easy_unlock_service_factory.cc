// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/easy_unlock_service_regular.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/easy_unlock_service_signin_chromeos.h"
#endif

// static
EasyUnlockServiceFactory* EasyUnlockServiceFactory::GetInstance() {
  return Singleton<EasyUnlockServiceFactory>::get();
}

// static
EasyUnlockService* EasyUnlockServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<EasyUnlockService*>(
      EasyUnlockServiceFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true));
}

EasyUnlockServiceFactory::EasyUnlockServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "EasyUnlockService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

EasyUnlockServiceFactory::~EasyUnlockServiceFactory() {
}

KeyedService* EasyUnlockServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(
          Profile::FromBrowserContext(context))) {
    if (EasyUnlockService::IsSignInEnabled()) {
      return new EasyUnlockServiceSignin(Profile::FromBrowserContext(context));
    } else {
      return NULL;
    }
  }
#endif
  return new EasyUnlockServiceRegular(Profile::FromBrowserContext(context));
}

content::BrowserContext* EasyUnlockServiceFactory::GetBrowserContextToUse(
      content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool EasyUnlockServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool EasyUnlockServiceFactory::ServiceIsNULLWhileTesting() const {
  // Don't create the service for TestingProfile used in unit_tests because
  // EasyUnlockService uses ExtensionSystem::ready().Post, which expects
  // a MessageLoop that does not exit in many unit_tests cases.
  return true;
}
