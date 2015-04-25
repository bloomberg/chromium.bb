// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_cast_mode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(MediaCastModeTest, PreferredCastMode) {
  CastModeSet cast_modes;

  EXPECT_EQ(MediaCastMode::DEFAULT, GetPreferredCastMode(cast_modes));

  cast_modes.insert(MediaCastMode::SOUND_OPTIMIZED_TAB_MIRROR);
  EXPECT_EQ(MediaCastMode::SOUND_OPTIMIZED_TAB_MIRROR,
            GetPreferredCastMode(cast_modes));

  cast_modes.insert(MediaCastMode::DESKTOP_OR_WINDOW_MIRROR);
  EXPECT_EQ(MediaCastMode::DESKTOP_OR_WINDOW_MIRROR,
            GetPreferredCastMode(cast_modes));

  cast_modes.insert(MediaCastMode::TAB_MIRROR);
  EXPECT_EQ(MediaCastMode::TAB_MIRROR,
            GetPreferredCastMode(cast_modes));

  cast_modes.insert(MediaCastMode::DEFAULT);
  EXPECT_EQ(MediaCastMode::DEFAULT,
            GetPreferredCastMode(cast_modes));

  cast_modes.erase(MediaCastMode::TAB_MIRROR);
  EXPECT_EQ(MediaCastMode::DEFAULT,
            GetPreferredCastMode(cast_modes));

  cast_modes.erase(MediaCastMode::DESKTOP_OR_WINDOW_MIRROR);
  EXPECT_EQ(MediaCastMode::DEFAULT,
            GetPreferredCastMode(cast_modes));

  cast_modes.erase(MediaCastMode::SOUND_OPTIMIZED_TAB_MIRROR);
  EXPECT_EQ(MediaCastMode::DEFAULT,
            GetPreferredCastMode(cast_modes));
}

TEST(MediaCastModeTest, MediaCastModeToTitleAndDescription) {
  for (int cast_mode = MediaCastMode::DEFAULT;
       cast_mode < MediaCastMode::NUM_CAST_MODES; cast_mode++) {
    EXPECT_TRUE(!MediaCastModeToTitle(
        static_cast<MediaCastMode>(cast_mode), "youtube.com").empty());
    EXPECT_TRUE(!MediaCastModeToDescription(
        static_cast<MediaCastMode>(cast_mode), "youtube.com").empty());
  }
}

TEST(MediaCastModeTest, IsValidCastModeNum) {
  for (int cast_mode = MediaCastMode::DEFAULT;
       cast_mode < MediaCastMode::NUM_CAST_MODES; cast_mode++) {
    EXPECT_TRUE(IsValidCastModeNum(cast_mode));
  }
  EXPECT_FALSE(IsValidCastModeNum(MediaCastMode::NUM_CAST_MODES));
  EXPECT_FALSE(IsValidCastModeNum(-1));
}

}  // namespace media_router
