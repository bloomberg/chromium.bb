// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/cast_modes_with_media_sources.h"

#include "chrome/browser/media/router/media_source_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media_router {

TEST(MediaRouterCastModesWithMediaSourcesTest, AddAndRemoveSources) {
  const MediaSource defaultSource1(MediaSourceForPresentationUrl(
      GURL("http://www.example.com/presentation.html")));
  const MediaSource defaultSource2(MediaSourceForPresentationUrl(
      GURL("http://www.example.net/presentation.html")));
  const MediaSource tabSourceA(MediaSourceForTab(123));
  const CastModeSet castModeSetEmpty;
  const CastModeSet castModeSetDefault({MediaCastMode::DEFAULT});
  const CastModeSet castModeSetTab({MediaCastMode::TAB_MIRROR});
  const CastModeSet castModeSetDefaultAndTab(
      {MediaCastMode::DEFAULT, MediaCastMode::TAB_MIRROR});

  CastModesWithMediaSources sources;
  EXPECT_TRUE(sources.IsEmpty());
  EXPECT_EQ(sources.GetCastModes(), castModeSetEmpty);

  // After the below addition, |sources| should contain:
  // [Default: 1]
  sources.AddSource(MediaCastMode::DEFAULT, defaultSource1);
  EXPECT_TRUE(sources.HasSource(MediaCastMode::DEFAULT, defaultSource1));
  EXPECT_FALSE(sources.HasSource(MediaCastMode::DEFAULT, defaultSource2));
  EXPECT_FALSE(sources.HasSource(MediaCastMode::TAB_MIRROR, defaultSource1));
  EXPECT_FALSE(sources.IsEmpty());
  EXPECT_EQ(sources.GetCastModes(), castModeSetDefault);

  // Trying to remove non-existing sources should be no-op.
  sources.RemoveSource(MediaCastMode::DEFAULT, defaultSource2);
  sources.RemoveSource(MediaCastMode::TAB_MIRROR, defaultSource1);
  sources.RemoveSource(MediaCastMode::TAB_MIRROR, tabSourceA);
  EXPECT_TRUE(sources.HasSource(MediaCastMode::DEFAULT, defaultSource1));
  EXPECT_EQ(sources.GetCastModes(), castModeSetDefault);

  // [Default: 1; Tab: A]
  sources.AddSource(MediaCastMode::TAB_MIRROR, tabSourceA);
  EXPECT_TRUE(sources.HasSource(MediaCastMode::DEFAULT, defaultSource1));
  EXPECT_TRUE(sources.HasSource(MediaCastMode::TAB_MIRROR, tabSourceA));
  EXPECT_EQ(sources.GetCastModes(), castModeSetDefaultAndTab);

  // [Default: 1,2; Tab: A]
  sources.AddSource(MediaCastMode::DEFAULT, defaultSource2);
  EXPECT_TRUE(sources.HasSource(MediaCastMode::DEFAULT, defaultSource2));
  EXPECT_EQ(sources.GetCastModes(), castModeSetDefaultAndTab);

  // [Default: 2; Tab: A]
  sources.RemoveSource(MediaCastMode::DEFAULT, defaultSource1);
  EXPECT_FALSE(sources.HasSource(MediaCastMode::DEFAULT, defaultSource1));
  EXPECT_TRUE(sources.HasSource(MediaCastMode::DEFAULT, defaultSource2));
  EXPECT_EQ(sources.GetCastModes(), castModeSetDefaultAndTab);

  // [Tab: A]
  sources.RemoveSource(MediaCastMode::DEFAULT, defaultSource2);
  EXPECT_FALSE(sources.HasSource(MediaCastMode::DEFAULT, defaultSource1));
  EXPECT_FALSE(sources.IsEmpty());
  EXPECT_EQ(sources.GetCastModes(), castModeSetTab);

  // []
  sources.RemoveSource(MediaCastMode::TAB_MIRROR, tabSourceA);
  EXPECT_FALSE(sources.HasSource(MediaCastMode::DEFAULT, defaultSource1));
  EXPECT_FALSE(sources.HasSource(MediaCastMode::DEFAULT, defaultSource2));
  EXPECT_FALSE(sources.HasSource(MediaCastMode::TAB_MIRROR, tabSourceA));
  EXPECT_TRUE(sources.IsEmpty());
  EXPECT_EQ(sources.GetCastModes(), castModeSetEmpty);
}

}  // namespace media_router
