// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_delegate_factory_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

TtsEngineDelegateFactoryImpl::TtsEngineDelegateFactoryImpl()
    : BrowserContextKeyedServiceFactory(
          "TtsEngineService",
          BrowserContextDependencyManager::GetInstance()) {}

TtsEngineDelegateFactoryImpl::~TtsEngineDelegateFactoryImpl() = default;

// static
TtsEngineDelegateImpl* TtsEngineDelegateFactoryImpl::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<TtsEngineDelegateImpl*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
TtsEngineDelegateFactoryImpl* TtsEngineDelegateFactoryImpl::GetInstance() {
  return base::Singleton<TtsEngineDelegateFactoryImpl>::get();
}

TtsEngineDelegate*
TtsEngineDelegateFactoryImpl::GetTtsEngineDelegateForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<TtsEngineDelegateImpl*>(
      GetServiceForBrowserContext(browser_context, true));
}

KeyedService* TtsEngineDelegateFactoryImpl::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new TtsEngineDelegateImpl(context);
}

bool TtsEngineDelegateFactoryImpl::ServiceIsNULLWhileTesting() const {
  return false;
}

bool TtsEngineDelegateFactoryImpl::ServiceIsCreatedWithBrowserContext() const {
  return false;
}
