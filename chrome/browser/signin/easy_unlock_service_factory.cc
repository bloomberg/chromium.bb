// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_service_factory.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/easy_unlock_app_manager.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/easy_unlock_service_regular.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/browser_resources.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/easy_unlock_service_signin_chromeos.h"
#endif

namespace {

// Gets the file path from which easy unlock app should be loaded.
base::FilePath GetEasyUnlockAppPath() {
#if defined(GOOGLE_CHROME_BUILD)
#ifndef NDEBUG
  // Only allow app path override switch for debug build.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEasyUnlockAppPath))
    return command_line->GetSwitchValuePath(switches::kEasyUnlockAppPath);
#endif  // !defined(NDEBUG)

#if defined(OS_CHROMEOS)
  return base::FilePath("/usr/share/chromeos-assets/easy_unlock");
#endif  // defined(OS_CHROMEOS)
#endif  // defined(GOOGLE_CHROME_BUILD)

  return base::FilePath();
}

}  // namespace

// static
EasyUnlockServiceFactory* EasyUnlockServiceFactory::GetInstance() {
  return base::Singleton<EasyUnlockServiceFactory>::get();
}

// static
EasyUnlockService* EasyUnlockServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<EasyUnlockService*>(
      EasyUnlockServiceFactory::GetInstance()->GetServiceForBrowserContext(
          browser_context, true));
}

EasyUnlockServiceFactory::EasyUnlockServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "EasyUnlockService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
#if defined(OS_CHROMEOS)
  DependsOn(EasyUnlockTpmKeyManagerFactory::GetInstance());
#endif
}

EasyUnlockServiceFactory::~EasyUnlockServiceFactory() {
}

KeyedService* EasyUnlockServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  EasyUnlockService* service = NULL;
  int manifest_id = 0;

#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(
          Profile::FromBrowserContext(context))) {
    if (!context->IsOffTheRecord())
      return NULL;

    service = new EasyUnlockServiceSignin(Profile::FromBrowserContext(context));
    manifest_id = IDR_EASY_UNLOCK_MANIFEST_SIGNIN;
  }
#endif

  if (!service) {
    service =
        new EasyUnlockServiceRegular(Profile::FromBrowserContext(context));
    manifest_id = IDR_EASY_UNLOCK_MANIFEST;
  }

  const base::FilePath app_path = app_path_for_testing_.empty()
                                      ? GetEasyUnlockAppPath()
                                      : app_path_for_testing_;

  service->Initialize(EasyUnlockAppManager::Create(
      extensions::ExtensionSystem::Get(context), manifest_id, app_path));
  return service;
}

content::BrowserContext* EasyUnlockServiceFactory::GetBrowserContextToUse(
      content::BrowserContext* context) const {
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(
          Profile::FromBrowserContext(context))) {
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }
#endif
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
