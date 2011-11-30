// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_util.h"

#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

TEST(ProfileInfoUtilTest, MenuIcon) {
  // Test that an avatar icon isn't changed.
  const gfx::Image& profile_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(IDR_PROFILE_AVATAR_0));
  gfx::Image result = profiles::GetAvatarIconForMenu(profile_image, false);
  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::IsEqual(profile_image, result));

  // Test that a GAIA picture is changed.
  gfx::Image gaia_picture(gfx::test::CreateImage());
  gfx::Image result2 = profiles::GetAvatarIconForMenu(gaia_picture, true);
  EXPECT_FALSE(gfx::test::IsEmpty(result2));
  EXPECT_FALSE(gfx::test::IsEqual(gaia_picture, result2));
}

TEST(ProfileInfoUtilTest, TitleBarIcon) {
  // Test that an avatar icon isn't changed.
  const gfx::Image& profile_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(IDR_PROFILE_AVATAR_0));
  gfx::Image result = profiles::GetAvatarIconForTitleBar(
      profile_image, false, 100, 40);
  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::IsEqual(profile_image, result));

  // Test that a GAIA picture is changed.
  gfx::Image gaia_picture(gfx::test::CreateImage());
  gfx::Image result2 = profiles::GetAvatarIconForTitleBar(
      gaia_picture, true, 100, 40);
  EXPECT_FALSE(gfx::test::IsEmpty(result2));
  EXPECT_FALSE(gfx::test::IsEqual(gaia_picture, result2));
}

}  // namespace
