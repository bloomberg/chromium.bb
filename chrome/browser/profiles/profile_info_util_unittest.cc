// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_util.h"

#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

TEST(ProfileInfoUtilTest, SizedMenuIcon) {
  // Test that an avatar icon isn't changed.
  const gfx::Image& profile_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(IDR_PROFILE_AVATAR_0));
  gfx::Image result =
      profiles::GetSizedAvatarIconWithBorder(profile_image, false, 50, 50);
  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::IsEqual(profile_image, result));

  // Test that a rectangular picture (e.g., GAIA image) is changed.
  gfx::Image rect_picture(gfx::test::CreateImage());
  gfx::Image result2 =
      profiles::GetSizedAvatarIconWithBorder(rect_picture, true, 50, 50);
  EXPECT_FALSE(gfx::test::IsEmpty(result2));
  EXPECT_FALSE(gfx::test::IsEqual(rect_picture, result2));
}

TEST(ProfileInfoUtilTest, MenuIcon) {
  // Test that an avatar icon isn't changed.
  const gfx::Image& profile_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(IDR_PROFILE_AVATAR_0));
  gfx::Image result = profiles::GetAvatarIconForMenu(profile_image, false);
  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::IsEqual(profile_image, result));

  // Test that a rectangular picture is changed.
  gfx::Image rect_picture(gfx::test::CreateImage());
  gfx::Image result2 = profiles::GetAvatarIconForMenu(rect_picture, true);
  EXPECT_FALSE(gfx::test::IsEmpty(result2));
  EXPECT_FALSE(gfx::test::IsEqual(rect_picture, result2));
}

TEST(ProfileInfoUtilTest, WebUIIcon) {
  // Test that an avatar icon isn't changed.
  const gfx::Image& profile_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(IDR_PROFILE_AVATAR_0));
  gfx::Image result = profiles::GetAvatarIconForWebUI(profile_image, false);
  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::IsEqual(profile_image, result));

  // Test that a rectangular picture is changed.
  gfx::Image rect_picture(gfx::test::CreateImage());
  gfx::Image result2 = profiles::GetAvatarIconForWebUI(rect_picture, true);
  EXPECT_FALSE(gfx::test::IsEmpty(result2));
  EXPECT_FALSE(gfx::test::IsEqual(rect_picture, result2));
}

TEST(ProfileInfoUtilTest, TitleBarIcon) {
  // Test that an avatar icon isn't changed.
  const gfx::Image& profile_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(IDR_PROFILE_AVATAR_0));
  gfx::Image result = profiles::GetAvatarIconForTitleBar(
      profile_image, false, 100, 40);
  EXPECT_FALSE(gfx::test::IsEmpty(result));
  EXPECT_TRUE(gfx::test::IsEqual(profile_image, result));

  // Test that a rectangular picture is changed.
  gfx::Image rect_picture(gfx::test::CreateImage());
  gfx::Image result2 = profiles::GetAvatarIconForTitleBar(
      rect_picture, true, 100, 40);
  EXPECT_FALSE(gfx::test::IsEmpty(result2));
  EXPECT_FALSE(gfx::test::IsEqual(rect_picture, result2));
}

}  // namespace
