// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/logo_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/logo_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
LogoService* LogoServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LogoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LogoServiceFactory* LogoServiceFactory::GetInstance() {
  return base::Singleton<LogoServiceFactory>::get();
}

LogoServiceFactory::LogoServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LogoService",
          BrowserContextDependencyManager::GetInstance()) {}

LogoServiceFactory::~LogoServiceFactory() = default;

KeyedService* LogoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(!profile->IsOffTheRecord());
  bool use_gray_background =
      !base::FeatureList::IsEnabled(chrome::android::kChromeHomeFeature);
  return new LogoService(profile, use_gray_background);
}
