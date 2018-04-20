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

// All platforms proxy for whether NTP shortcuts are enabled.
bool AreNtpShortcutsEnabled() {
#if defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(chrome::android::kNTPShortcuts);
#else
  return false;
#endif  // OS_ANDROID
}

bool AreAssetDownloadsEnabled() {
  return !AreNtpShortcutsEnabled() &&
         base::FeatureList::IsEnabled(
             features::kAssetDownloadSuggestionsFeature);
}

bool AreOfflinePageDownloadsEnabled() {
  return !AreNtpShortcutsEnabled() &&
         base::FeatureList::IsEnabled(
             features::kOfflinePageDownloadSuggestionsFeature);
}
bool IsDownloadsProviderEnabled() {
  return AreAssetDownloadsEnabled() || AreOfflinePageDownloadsEnabled();
}

bool IsBookmarkProviderEnabled() {
  return !AreNtpShortcutsEnabled() &&
         base::FeatureList::IsEnabled(
             ntp_snippets::kBookmarkSuggestionsFeature);
}

bool IsRecentTabProviderEnabled() {
  return !AreNtpShortcutsEnabled() &&
         base::FeatureList::IsEnabled(
             ntp_snippets::kRecentOfflineTabSuggestionsFeature) &&
         base::FeatureList::IsEnabled(
             offline_pages::kOffliningRecentPagesFeature);
}

bool IsForeignSessionsProviderEnabled() {
  return !AreNtpShortcutsEnabled() &&
         base::FeatureList::IsEnabled(
             ntp_snippets::kForeignSessionsSuggestionsFeature);
}

}  // namespace ntp_snippets
