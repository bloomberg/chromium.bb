// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flags_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace {

const char kFieldTrialName[] = "EnhancedBookmarks";

}  // namespace

bool GetBookmarksExperimentExtensionID(std::string* extension_id) {
  *extension_id = variations::GetVariationParamValue(kFieldTrialName, "id");
  if (extension_id->empty())
    return false;

  // kEnhancedBookmarksExperiment flag could have values "", "1" and "0".
  // "0" - user opted out.
  bool opt_out = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kEnhancedBookmarksExperiment) == "0";

  if (opt_out)
    return false;

#if defined(OS_ANDROID)
  return base::android::BuildInfo::GetInstance()->sdk_int() >
         base::android::SdkVersion::SDK_VERSION_ICE_CREAM_SANDWICH_MR1;
#else
  const extensions::FeatureProvider* feature_provider =
      extensions::FeatureProvider::GetPermissionFeatures();
  extensions::Feature* feature = feature_provider->GetFeature("metricsPrivate");
  return feature && feature->IsIdInWhitelist(*extension_id);
#endif
}

#if defined(OS_ANDROID)
bool IsEnhancedBookmarkImageFetchingEnabled(const PrefService* user_prefs) {
  if (IsEnhancedBookmarksEnabled())
    return true;

  // Salient images are collected from visited bookmarked pages even if the
  // enhanced bookmark feature is turned off. This is to have some images
  // available so that in the future, when the feature is turned on, the user
  // experience is not a big list of flat colors. However as a precautionary
  // measure it is possible to disable this collection of images from finch.
  std::string disable_fetching = variations::GetVariationParamValue(
      kFieldTrialName, "DisableImagesFetching");
  return disable_fetching.empty();
}

bool IsEnhancedBookmarksEnabled() {
  std::string extension_id;
  return GetBookmarksExperimentExtensionID(&extension_id);
}
#endif

bool IsEnableDomDistillerSet() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDomDistiller)) {
    return true;
  }
  if (variations::GetVariationParamValue(
          kFieldTrialName, "enable-dom-distiller") == "1")
    return true;

  return false;
}

bool IsEnableSyncArticlesSet() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSyncArticles)) {
    return true;
  }
  if (variations::GetVariationParamValue(
          kFieldTrialName, "enable-sync-articles") == "1")
    return true;

  return false;
}
