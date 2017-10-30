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
#include "components/search_provider_logos/logo_service_impl.h"
#include "net/url_request/url_request_context_getter.h"

using search_provider_logos::LogoService;
using search_provider_logos::LogoServiceImpl;

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
  // TODO(https://crbug.com/776725): Update LogoServiceImpl to request the
  // latest value instead of caching it. Depending on the UI mode (ChromeHome
  // enabled or not), the background where the logo is used changes from gray to
  // white. This value can change at runtime, so we should stop using a value
  // determined at factory instantiation time.
  bool use_gray_background = true;
#else
  bool use_gray_background = false;
#endif
  return new LogoServiceImpl(profile->GetPath().Append(kCachedLogoDirectory),
                             TemplateURLServiceFactory::GetForProfile(profile),
                             base::MakeUnique<suggestions::ImageDecoderImpl>(),
                             profile->GetRequestContext(), use_gray_background);
}
