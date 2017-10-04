// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_feature.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"

namespace switches {

// This flag significantly shortens the delay between WebContentsObserver events
// and SnapshotController's StartSnapshot calls. The purpose is to speed up
// integration tests.
const char kOfflinePagesUseTestingSnapshotDelay[] =
    "short-offline-page-snapshot-delay-for-test";

}  // namespace switches

namespace offline_pages {

const base::Feature kOfflineBookmarksFeature{"OfflineBookmarks",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOffliningRecentPagesFeature{
    "OfflineRecentPages", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOfflinePagesCTFeature{"OfflinePagesCT",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOfflinePagesSharingFeature{
    "OfflinePagesSharing", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOfflinePagesSvelteConcurrentLoadingFeature{
    "OfflinePagesSvelteConcurrentLoading", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOfflinePagesLoadSignalCollectingFeature{
    "OfflinePagesLoadSignalCollecting", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOfflinePagesRenovationsFeature{
    "OfflinePagesRenovations", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOfflinePagesResourceBasedSnapshotFeature{
    "OfflinePagesResourceBasedSnapshot", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBackgroundLoaderForDownloadsFeature{
    "BackgroundLoadingForDownloads", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOfflinePagesAsyncDownloadFeature{
    "OfflinePagesAsyncDownload", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPrefetchingOfflinePagesFeature{
    "OfflinePagesPrefetching", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOfflinePagesCTV2Feature{"OfflinePagesCTV2",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOfflinePagesPrefetchingUIFeature{
    "OfflinePagesPrefetchingUI", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsOfflineBookmarksEnabled() {
  return base::FeatureList::IsEnabled(kOfflineBookmarksFeature);
}

bool IsOffliningRecentPagesEnabled() {
  return base::FeatureList::IsEnabled(kOffliningRecentPagesFeature);
}

bool IsOfflinePagesSvelteConcurrentLoadingEnabled() {
  return base::FeatureList::IsEnabled(
      kOfflinePagesSvelteConcurrentLoadingFeature);
}

bool IsOfflinePagesCTEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesCTFeature);
}

bool IsOfflinePagesSharingEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesSharingFeature);
}

bool IsBackgroundLoaderForDownloadsEnabled() {
  return base::FeatureList::IsEnabled(kBackgroundLoaderForDownloadsFeature);
}

bool IsOfflinePagesAsyncDownloadEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesAsyncDownloadFeature);
}

bool IsPrefetchingOfflinePagesEnabled() {
  return base::FeatureList::IsEnabled(kPrefetchingOfflinePagesFeature);
}

bool IsOfflinePagesPrefetchingUIEnabled() {
  return IsPrefetchingOfflinePagesEnabled() &&
         base::FeatureList::IsEnabled(kOfflinePagesPrefetchingUIFeature);
}

bool IsOfflinePagesLoadSignalCollectingEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesLoadSignalCollectingFeature);
}

bool IsOfflinePagesRenovationsEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesRenovationsFeature);
}

bool IsOfflinePagesResourceBasedSnapshotEnabled() {
  return base::FeatureList::IsEnabled(
      kOfflinePagesResourceBasedSnapshotFeature);
}

bool ShouldUseTestingSnapshotDelay() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  return cl->HasSwitch(switches::kOfflinePagesUseTestingSnapshotDelay);
}

bool IsOfflinePagesCTV2Enabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesCTV2Feature);
}

}  // namespace offline_pages
