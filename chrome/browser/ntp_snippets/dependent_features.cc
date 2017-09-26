// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/dependent_features.h"

#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "components/ntp_snippets/features.h"
#include "components/offline_pages/core/offline_page_feature.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/feature_utilities.h"
#endif  // OS_ANDROID

namespace ntp_snippets {

namespace {

// All platforms proxy for chrome::android::GetIsChromeHomeEnabled.
bool GetIsChromeHomeEnabled() {
#if defined(OS_ANDROID)
  return chrome::android::GetIsChromeHomeEnabled();
#else
  return false;
#endif  // OS_ANDROID
}

bool IsPhysicalWebEnabled() {
#if defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(chrome::android::kPhysicalWebFeature);
#else
  return false;
#endif  // OS_ANDROID
}

}  // namespace

bool AreAssetDownloadsEnabled() {
  return !GetIsChromeHomeEnabled() &&
         base::FeatureList::IsEnabled(
             features::kAssetDownloadSuggestionsFeature);
}

bool AreOfflinePageDownloadsEnabled() {
  return !GetIsChromeHomeEnabled() &&
         base::FeatureList::IsEnabled(
             features::kOfflinePageDownloadSuggestionsFeature);
}
bool IsDownloadsProviderEnabled() {
  return AreAssetDownloadsEnabled() || AreOfflinePageDownloadsEnabled();
}

bool IsBookmarkProviderEnabled() {
  return !GetIsChromeHomeEnabled() &&
         base::FeatureList::IsEnabled(
             ntp_snippets::kBookmarkSuggestionsFeature);
}

bool IsRecentTabProviderEnabled() {
  return !GetIsChromeHomeEnabled() &&
         base::FeatureList::IsEnabled(
             ntp_snippets::kRecentOfflineTabSuggestionsFeature) &&
         base::FeatureList::IsEnabled(
             offline_pages::kOffliningRecentPagesFeature);
}

bool IsPhysicalWebPageProviderEnabled() {
  return !GetIsChromeHomeEnabled() &&
         base::FeatureList::IsEnabled(
             ntp_snippets::kPhysicalWebPageSuggestionsFeature) &&
         IsPhysicalWebEnabled();
}

bool IsForeignSessionsProviderEnabled() {
  return !GetIsChromeHomeEnabled() &&
         base::FeatureList::IsEnabled(
             ntp_snippets::kForeignSessionsSuggestionsFeature);
}

}  // namespace ntp_snippets
