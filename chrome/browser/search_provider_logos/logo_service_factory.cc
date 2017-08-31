// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_provider_logos/logo_service_factory.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_provider_logos/logo_service.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/feature_utilities.h"
#endif

using search_provider_logos::LogoService;

#if defined(OS_ANDROID)
using chrome::android::GetIsChromeHomeEnabled;
#endif  // defined(OS_ANDROID)

namespace {

constexpr base::FilePath::CharType kCachedLogoDirectory[] =
    FILE_PATH_LITERAL("Search Logos");

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
#if defined(OS_ANDROID)
  bool use_gray_background = !GetIsChromeHomeEnabled();
#else
  bool use_gray_background = false;
#endif
  return new LogoService(profile->GetPath().Append(kCachedLogoDirectory),
                         TemplateURLServiceFactory::GetForProfile(profile),
                         base::MakeUnique<suggestions::ImageDecoderImpl>(),
                         profile->GetRequestContext(), use_gray_background);
}
