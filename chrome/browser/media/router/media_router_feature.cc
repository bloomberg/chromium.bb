// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_feature.h"

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "extensions/features/features.h"

#if defined(ENABLE_MEDIA_ROUTER)
#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // defined(ENABLE_MEDIA_ROUTER)

namespace media_router {

#if defined(ENABLE_MEDIA_ROUTER)
#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
namespace {
const PrefService::Preference* GetMediaRouterPref(
    content::BrowserContext* context) {
  return user_prefs::UserPrefs::Get(context)
      ->FindPreference(prefs::kEnableMediaRouter);
}
}  // namespace
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // defined(ENABLE_MEDIA_ROUTER)

bool MediaRouterEnabled(content::BrowserContext* context) {
#if defined(ENABLE_MEDIA_ROUTER)
#if defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
  const PrefService::Preference* pref = GetMediaRouterPref(context);
  // Only use the pref value if it set from a mandatory policy.
  if (pref->IsManaged() && !pref->IsDefaultValue()) {
    bool allowed = false;
    CHECK(pref->GetValue()->GetAsBoolean(&allowed));
    return allowed;
  }
  return true;
#else  // !(defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS))
  return false;
#endif  // defined(OS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS)
#else   // !defined(ENABLE_MEDIA_ROUTER)
  return false;
#endif  // defined(ENABLE_MEDIA_ROUTER)
}

}  // namespace media_router
