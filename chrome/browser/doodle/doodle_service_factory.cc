// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/doodle/doodle_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/doodle/doodle_fetcher.h"
#include "components/doodle/doodle_fetcher_impl.h"
#include "components/doodle/doodle_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_json/safe_json_parser.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#endif

#if defined(OS_ANDROID)
namespace {
const char kOverrideUrlParam[] = "doodle_override_url";
}  // namespace
#endif

// static
DoodleServiceFactory* DoodleServiceFactory::GetInstance() {
  return base::Singleton<DoodleServiceFactory>::get();
}

// static
doodle::DoodleService* DoodleServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<doodle::DoodleService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

DoodleServiceFactory::DoodleServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DoodleService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(GoogleURLTrackerFactory::GetInstance());
}

DoodleServiceFactory::~DoodleServiceFactory() = default;

KeyedService* DoodleServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  // We don't show doodles in incognito profiles (for now?).
  DCHECK(!profile->IsOffTheRecord());

  bool use_gray_background = false;
  base::Optional<std::string> override_url;

#if defined(OS_ANDROID)
  DCHECK(base::FeatureList::IsEnabled(chrome::android::kUseNewDoodleApi));

  use_gray_background =
      !base::FeatureList::IsEnabled(chrome::android::kChromeHomeFeature);

  std::string override_url_str = base::GetFieldTrialParamValueByFeature(
      chrome::android::kUseNewDoodleApi, kOverrideUrlParam);
  // GetFieldTrialParamValueByFeature returns an empty string if the param is
  // not set.
  if (!override_url_str.empty()) {
    override_url = override_url_str;
  }
#endif

  auto fetcher = base::MakeUnique<doodle::DoodleFetcherImpl>(
      profile->GetRequestContext(),
      GoogleURLTrackerFactory::GetForProfile(profile),
      base::Bind(&safe_json::SafeJsonParser::Parse), use_gray_background,
      override_url);
  return new doodle::DoodleService(
      profile->GetPrefs(), std::move(fetcher),
      base::MakeUnique<base::OneShotTimer>(),
      base::MakeUnique<base::DefaultClock>(),
      base::MakeUnique<base::DefaultTickClock>(),
      /*override_min_refresh_interval=*/base::nullopt);
}
