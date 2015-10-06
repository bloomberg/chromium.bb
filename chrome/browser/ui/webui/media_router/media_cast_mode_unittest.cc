// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_cast_mode.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Not;
using testing::HasSubstr;

namespace media_router {

TEST(MediaCastModeTest, PreferredCastMode) {
  CastModeSet cast_modes;

  EXPECT_EQ(MediaCastMode::DEFAULT, GetPreferredCastMode(cast_modes));

  cast_modes.insert(MediaCastMode::DESKTOP_MIRROR);
  EXPECT_EQ(MediaCastMode::DESKTOP_MIRROR,
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

  cast_modes.erase(MediaCastMode::DESKTOP_MIRROR);
  EXPECT_EQ(MediaCastMode::DEFAULT,
            GetPreferredCastMode(cast_modes));
}

TEST(MediaCastModeTest, MediaCastModeToDescription) {
  for (int cast_mode = MediaCastMode::DEFAULT;
       cast_mode < MediaCastMode::NUM_CAST_MODES; cast_mode++) {
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
