// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_feature.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

TEST(OfflinePageFeatureTest, OfflineBookmarks) {
  // Enabled by default.
  EXPECT_TRUE(offline_pages::IsOfflineBookmarksEnabled());

  // Check if feature is correctly disabled by command-line flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kOfflineBookmarksFeature);
  EXPECT_FALSE(offline_pages::IsOfflineBookmarksEnabled());
}

TEST(OfflinePageFeatureTest, OffliningRecentPages) {
  // Enabled by default.
  EXPECT_TRUE(offline_pages::IsOffliningRecentPagesEnabled());

  // Check if feature is correctly disabled by command-line flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kOffliningRecentPagesFeature);
  EXPECT_FALSE(offline_pages::IsOffliningRecentPagesEnabled());
}

TEST(OfflinePageFeatureTest, OfflinePagesSharing) {
  // This feature is disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflinePagesSharingEnabled());

  // Check if feature is correctly disabled by command-line flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kOfflinePagesSharingFeature);
  EXPECT_TRUE(offline_pages::IsOfflinePagesSharingEnabled());
}

TEST(OfflinePageFeatureTest, OfflinePagesSvelteConcurrentLoading) {
  // This feature is disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflinePagesSvelteConcurrentLoadingEnabled());

  // Check if feature is correctly enabled by command-line flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOfflinePagesSvelteConcurrentLoadingFeature);
  EXPECT_TRUE(offline_pages::IsOfflinePagesSvelteConcurrentLoadingEnabled());
}

TEST(OfflinePageFeatureTest, OfflinePagesLoadSignalCollecting) {
  // This feature is disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflinePagesLoadSignalCollectingEnabled());

  // Check if feature is correctly enabled by command-line flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOfflinePagesLoadSignalCollectingFeature);
  EXPECT_TRUE(offline_pages::IsOfflinePagesLoadSignalCollectingEnabled());
}

}  // namespace offline_pages
