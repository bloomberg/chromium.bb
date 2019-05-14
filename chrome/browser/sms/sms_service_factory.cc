// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sms/sms_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/sms/sms_keyed_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
SmsKeyedService* SmsServiceFactory::GetForProfile(
    content::BrowserContext* profile) {
  return static_cast<SmsKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SmsServiceFactory* SmsServiceFactory::GetInstance() {
  static base::NoDestructor<SmsServiceFactory> instance;
  return instance.get();
}

SmsServiceFactory::SmsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SmsService",
          BrowserContextDependencyManager::GetInstance()) {}

SmsServiceFactory::~SmsServiceFactory() = default;

content::BrowserContext* SmsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* SmsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SmsKeyedService();
}
