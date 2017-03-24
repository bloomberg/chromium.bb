// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cryptauth/chrome_cryptauth_service_factory.h"

#include "chrome/browser/cryptauth/chrome_cryptauth_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
cryptauth::CryptAuthService* CryptAuthServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ChromeCryptAuthService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
CryptAuthServiceFactory* CryptAuthServiceFactory::GetInstance() {
  return base::Singleton<CryptAuthServiceFactory>::get();
}

CryptAuthServiceFactory::CryptAuthServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CryptAuthService",
          BrowserContextDependencyManager::GetInstance()) {}

CryptAuthServiceFactory::~CryptAuthServiceFactory() {}

KeyedService* CryptAuthServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return ChromeCryptAuthService::Create(profile).release();
}
