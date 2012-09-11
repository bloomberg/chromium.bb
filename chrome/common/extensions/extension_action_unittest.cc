// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_action.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "webkit/glue/image_decoder.h"

namespace {

bool ImagesAreEqual(const gfx::Image& i1, const gfx::Image& i2) {
  return gfx::BitmapsAreEqual(*i1.ToSkBitmap(), *i2.ToSkBitmap());
}

gfx::Image LoadIcon(const std::string& filename) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions").AppendASCII(filename);

  std::string file_contents;
  file_util::ReadFileToString(path, &file_contents);
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(file_contents.data());

  SkBitmap bitmap;
  webkit_glue::ImageDecoder decoder;
  bitmap = decoder.Decode(data, file_contents.length());

  return gfx::Image(bitmap);
}

class ExtensionActionTest : public testing::Test {
 public:
  ExtensionActionTest()
      : action("", ExtensionAction::TYPE_PAGE) {
  }

  ExtensionAction action;
};

TEST_F(ExtensionActionTest, Title) {
  ASSERT_EQ("", action.GetTitle(1));
  action.SetTitle(ExtensionAction::kDefaultTabId, "foo");
  ASSERT_EQ("foo", action.GetTitle(1));
  ASSERT_EQ("foo", action.GetTitle(100));
  action.SetTitle(100, "bar");
  ASSERT_EQ("foo", action.GetTitle(1));
  ASSERT_EQ("bar", action.GetTitle(100));
  action.SetTitle(ExtensionAction::kDefaultTabId, "baz");
  ASSERT_EQ("baz", action.GetTitle(1));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ("baz", action.GetTitle(100));
}

TEST_F(ExtensionActionTest, Icon) {
  gfx::Image puzzle_piece =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_EXTENSIONS_FAVICON);
  gfx::Image icon1 = LoadIcon("icon1.png");
  gfx::Image icon2 = LoadIcon("icon2.png");
  ASSERT_TRUE(ImagesAreEqual(puzzle_piece, action.GetIcon(1)));

  action.set_default_icon_path("the_default.png");
  ASSERT_TRUE(ImagesAreEqual(puzzle_piece, action.GetIcon(1)))
      << "Still returns the puzzle piece because the image isn't loaded yet.";
  action.CacheIcon("the_default.png", icon2);
  ASSERT_TRUE(ImagesAreEqual(icon2, action.GetIcon(1)));

  action.SetIcon(ExtensionAction::kDefaultTabId, icon1);
  ASSERT_TRUE(ImagesAreEqual(icon1, action.GetIcon(100)))
      << "SetIcon(kDefaultTabId) overrides the default_icon_path.";

  action.SetIcon(100, icon2);
  ASSERT_TRUE(ImagesAreEqual(icon1, action.GetIcon(1)));
  ASSERT_TRUE(ImagesAreEqual(icon2, action.GetIcon(100)));
}

TEST_F(ExtensionActionTest, IconIndex) {
  gfx::Image icon1 = LoadIcon("icon1.png");
  gfx::Image icon2 = LoadIcon("icon2.png");
  gfx::Image puzzle_piece =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_EXTENSIONS_FAVICON);

  ASSERT_EQ(-1, action.GetIconIndex(1));
  action.icon_paths()->push_back("foo.png");
  action.icon_paths()->push_back("bar.png");
  action.SetIconIndex(ExtensionAction::kDefaultTabId, 1);
  ASSERT_EQ(1, action.GetIconIndex(1));
  ASSERT_EQ(1, action.GetIconIndex(100));
  ASSERT_TRUE(ImagesAreEqual(puzzle_piece, action.GetIcon(100)))
      << "Non-loaded icon gets the puzzle piece.";
  action.CacheIcon("bar.png", icon1);
  ASSERT_TRUE(ImagesAreEqual(icon1, action.GetIcon(100)));

  action.SetIconIndex(100, 0);
  ASSERT_EQ(0, action.GetIconIndex(100));
  ASSERT_EQ(1, action.GetIconIndex(1));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ(1, action.GetIconIndex(100));
  ASSERT_EQ(1, action.GetIconIndex(1));
}

TEST_F(ExtensionActionTest, Visibility) {
  // Supports the icon animation.
  MessageLoop message_loop;

  ASSERT_FALSE(action.GetIsVisible(1));
  EXPECT_FALSE(action.GetIconAnimation(ExtensionAction::kDefaultTabId));
  action.SetAppearance(ExtensionAction::kDefaultTabId, ExtensionAction::ACTIVE);
  ASSERT_TRUE(action.GetIsVisible(1));
  ASSERT_TRUE(action.GetIsVisible(100));
  EXPECT_FALSE(action.GetIconAnimation(ExtensionAction::kDefaultTabId));

  action.SetAppearance(ExtensionAction::kDefaultTabId,
                       ExtensionAction::INVISIBLE);
  ASSERT_FALSE(action.GetIsVisible(1));
  ASSERT_FALSE(action.GetIsVisible(100));
  EXPECT_FALSE(action.GetIconAnimation(100));
  action.SetAppearance(100, ExtensionAction::ACTIVE);
  ASSERT_FALSE(action.GetIsVisible(1));
  ASSERT_TRUE(action.GetIsVisible(100));
  EXPECT_TRUE(action.GetIconAnimation(100));

  action.ClearAllValuesForTab(100);
  ASSERT_FALSE(action.GetIsVisible(1));
  ASSERT_FALSE(action.GetIsVisible(100));
  EXPECT_FALSE(action.GetIconAnimation(100));
}

TEST_F(ExtensionActionTest, GetAttention) {
  // Supports the icon animation.
  scoped_ptr<MessageLoop> message_loop(new MessageLoop);

  EXPECT_FALSE(action.GetIsVisible(1));
  EXPECT_FALSE(action.GetIconAnimation(1));
  action.SetAppearance(1, ExtensionAction::WANTS_ATTENTION);
  EXPECT_TRUE(action.GetIsVisible(1));
  EXPECT_TRUE(action.GetIconAnimation(1));

  // Simulate waiting long enough for the animation to end.
  message_loop.reset();  // Can't have 2 MessageLoops alive at once.
  message_loop.reset(new MessageLoop);
  EXPECT_FALSE(action.GetIconAnimation(1));  // Sanity check.

  action.SetAppearance(1, ExtensionAction::ACTIVE);
  EXPECT_FALSE(action.GetIconAnimation(1))
      << "The animation should not play again if the icon was already visible.";
}

TEST_F(ExtensionActionTest, Badge) {
  ASSERT_EQ("", action.GetBadgeText(1));
  action.SetBadgeText(ExtensionAction::kDefaultTabId, "foo");
  ASSERT_EQ("foo", action.GetBadgeText(1));
  ASSERT_EQ("foo", action.GetBadgeText(100));
  action.SetBadgeText(100, "bar");
  ASSERT_EQ("foo", action.GetBadgeText(1));
  ASSERT_EQ("bar", action.GetBadgeText(100));
  action.SetBadgeText(ExtensionAction::kDefaultTabId, "baz");
  ASSERT_EQ("baz", action.GetBadgeText(1));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ("baz", action.GetBadgeText(100));
}

TEST_F(ExtensionActionTest, BadgeTextColor) {
  ASSERT_EQ(0x00000000u, action.GetBadgeTextColor(1));
  action.SetBadgeTextColor(ExtensionAction::kDefaultTabId, 0xFFFF0000u);
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeTextColor(1));
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeTextColor(100));
  action.SetBadgeTextColor(100, 0xFF00FF00);
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeTextColor(1));
  ASSERT_EQ(0xFF00FF00u, action.GetBadgeTextColor(100));
  action.SetBadgeTextColor(ExtensionAction::kDefaultTabId, 0xFF0000FFu);
  ASSERT_EQ(0xFF0000FFu, action.GetBadgeTextColor(1));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ(0xFF0000FFu, action.GetBadgeTextColor(100));
}

TEST_F(ExtensionActionTest, BadgeBackgroundColor) {
  ASSERT_EQ(0x00000000u, action.GetBadgeBackgroundColor(1));
  action.SetBadgeBackgroundColor(ExtensionAction::kDefaultTabId,
                                 0xFFFF0000u);
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeBackgroundColor(1));
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeBackgroundColor(100));
  action.SetBadgeBackgroundColor(100, 0xFF00FF00);
  ASSERT_EQ(0xFFFF0000u, action.GetBadgeBackgroundColor(1));
  ASSERT_EQ(0xFF00FF00u, action.GetBadgeBackgroundColor(100));
  action.SetBadgeBackgroundColor(ExtensionAction::kDefaultTabId,
                                 0xFF0000FFu);
  ASSERT_EQ(0xFF0000FFu, action.GetBadgeBackgroundColor(1));
  action.ClearAllValuesForTab(100);
  ASSERT_EQ(0xFF0000FFu, action.GetBadgeBackgroundColor(100));
}

TEST_F(ExtensionActionTest, PopupUrl) {
  GURL url_unset;
  GURL url_foo("http://www.example.com/foo.html");
  GURL url_bar("http://www.example.com/bar.html");
  GURL url_baz("http://www.example.com/baz.html");

  ASSERT_EQ(url_unset, action.GetPopupUrl(1));
  ASSERT_EQ(url_unset, action.GetPopupUrl(100));
  ASSERT_FALSE(action.HasPopup(1));
  ASSERT_FALSE(action.HasPopup(100));

  action.SetPopupUrl(ExtensionAction::kDefaultTabId, url_foo);
  ASSERT_EQ(url_foo, action.GetPopupUrl(1));
  ASSERT_EQ(url_foo, action.GetPopupUrl(100));

  action.SetPopupUrl(100, url_bar);
  ASSERT_EQ(url_foo, action.GetPopupUrl(1));
  ASSERT_EQ(url_bar, action.GetPopupUrl(100));

  action.SetPopupUrl(ExtensionAction::kDefaultTabId, url_baz);
  ASSERT_EQ(url_baz, action.GetPopupUrl(1));
  ASSERT_EQ(url_bar, action.GetPopupUrl(100));

  action.ClearAllValuesForTab(100);
  ASSERT_EQ(url_baz, action.GetPopupUrl(1));
  ASSERT_EQ(url_baz, action.GetPopupUrl(100));
}

}  // namespace
