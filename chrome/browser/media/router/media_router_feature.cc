// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_feature.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/features/features.h"

#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)

namespace media_router {

#if !defined(OS_ANDROID)
// Controls if browser side DIAL device discovery is enabled.
const base::Feature kEnableDialLocalDiscovery{
    "EnableDialLocalDiscovery", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if browser side Cast device discovery is enabled.
const base::Feature kEnableCastDiscovery{"EnableCastDiscovery",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls if local media casting is enabled.
const base::Feature kEnableCastLocalMedia{"EnableCastLocalMedia",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
namespace {
const PrefService::Preference* GetMediaRouterPref(
    content::BrowserContext* context) {
  return user_prefs::UserPrefs::Get(context)->FindPreference(
      prefs::kEnableMediaRouter);
}
}  // namespace
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)

bool MediaRouterEnabled(content::BrowserContext* context) {
#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
  const PrefService::Preference* pref = GetMediaRouterPref(context);
  // Only use the pref value if it set from a mandatory policy.
  if (pref->IsManaged() && !pref->IsDefaultValue()) {
    bool allowed = false;
    CHECK(pref->GetValue()->GetAsBoolean(&allowed));
    return allowed;
  }

  // The component extension cannot be loaded in guest sessions.
  // TODO(crbug.com/756243): Figure out why.
  return !Profile::FromBrowserContext(context)->IsGuestSession();
#else  // !(defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS))
  return false;
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
}

#if !defined(OS_ANDROID)
// Returns true if browser side DIAL discovery is enabled.
bool DialLocalDiscoveryEnabled() {
  return base::FeatureList::IsEnabled(kEnableDialLocalDiscovery);
}

// Returns true if browser side Cast discovery is enabled.
bool CastDiscoveryEnabled() {
  return base::FeatureList::IsEnabled(kEnableCastDiscovery);
}

// Returns true if local media casting is enabled.
bool CastLocalMediaEnabled() {
  return base::FeatureList::IsEnabled(kEnableCastLocalMedia);
}

#endif

}  // namespace media_router
