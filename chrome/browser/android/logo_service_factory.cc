// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/logo_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_provider_logos/logo_service.h"
#include "net/url_request/url_request_context_getter.h"

using search_provider_logos::LogoService;

namespace {

const char kCachedLogoDirectory[] = "Search Logos";

}  // namespace

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
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

LogoServiceFactory::~LogoServiceFactory() = default;

KeyedService* LogoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(!profile->IsOffTheRecord());
  bool use_gray_background =
      !base::FeatureList::IsEnabled(chrome::android::kChromeHomeFeature);
  return new LogoService(profile->GetPath().Append(kCachedLogoDirectory),
                         TemplateURLServiceFactory::GetForProfile(profile),
                         base::MakeUnique<suggestions::ImageDecoderImpl>(),
                         profile->GetRequestContext(), use_gray_background);
}
