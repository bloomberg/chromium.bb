// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using extensions::ActionInfo;

TEST(ExtensionActionTest, Title) {
  ActionInfo action_info;
  action_info.default_title = "Initial Title";
  ExtensionAction action("", ActionInfo::TYPE_PAGE, action_info);

  ASSERT_EQ("Initial Title", action.GetTitle(1));
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

TEST(ExtensionActionTest, Visibility) {
  ExtensionAction action("", ActionInfo::TYPE_PAGE,
                         ActionInfo());

  ASSERT_FALSE(action.GetIsVisible(1));
  action.SetAppearance(ExtensionAction::kDefaultTabId, ExtensionAction::ACTIVE);
  ASSERT_TRUE(action.GetIsVisible(1));
  ASSERT_TRUE(action.GetIsVisible(100));

  action.SetAppearance(ExtensionAction::kDefaultTabId,
                       ExtensionAction::INVISIBLE);
  ASSERT_FALSE(action.GetIsVisible(1));
  ASSERT_FALSE(action.GetIsVisible(100));
  action.SetAppearance(100, ExtensionAction::ACTIVE);
  ASSERT_FALSE(action.GetIsVisible(1));
  ASSERT_TRUE(action.GetIsVisible(100));
  EXPECT_FALSE(action.GetIconAnimation(100))
      << "Page actions should not animate.";

  action.ClearAllValuesForTab(100);
  ASSERT_FALSE(action.GetIsVisible(1));
  ASSERT_FALSE(action.GetIsVisible(100));

  ExtensionAction browser_action("", ActionInfo::TYPE_BROWSER,
                                 ActionInfo());
  ASSERT_TRUE(browser_action.GetIsVisible(1));
}

TEST(ExtensionActionTest, ScriptBadgeAnimation) {
  // Supports the icon animation.
  MessageLoop message_loop;

  ExtensionAction script_badge("", ActionInfo::TYPE_SCRIPT_BADGE,
                               ActionInfo());
  EXPECT_FALSE(script_badge.GetIconAnimation(ExtensionAction::kDefaultTabId));
  script_badge.SetAppearance(ExtensionAction::kDefaultTabId,
                             ExtensionAction::ACTIVE);
  EXPECT_FALSE(script_badge.GetIconAnimation(ExtensionAction::kDefaultTabId))
      << "Showing the default tab should not animate script badges.";

  script_badge.SetAppearance(ExtensionAction::kDefaultTabId,
                             ExtensionAction::INVISIBLE);
  EXPECT_FALSE(script_badge.GetIconAnimation(1))
      << "Making a script badge invisible should not show its animation.";
  script_badge.SetAppearance(1, ExtensionAction::ACTIVE);
  EXPECT_TRUE(script_badge.GetIconAnimation(1))
      << "Making a script badge visible should show its animation.";

  script_badge.ClearAllValuesForTab(1);
  EXPECT_FALSE(script_badge.GetIconAnimation(100));
}

TEST(ExtensionActionTest, GetAttention) {
  // Supports the icon animation.
  scoped_ptr<MessageLoop> message_loop(new MessageLoop);

  ExtensionAction script_badge("", ActionInfo::TYPE_SCRIPT_BADGE,
                               ActionInfo());
  EXPECT_FALSE(script_badge.GetIsVisible(1));
  EXPECT_FALSE(script_badge.GetIconAnimation(1));
  script_badge.SetAppearance(1, ExtensionAction::WANTS_ATTENTION);
  EXPECT_TRUE(script_badge.GetIsVisible(1));
  EXPECT_TRUE(script_badge.GetIconAnimation(1));

  // Simulate waiting long enough for the animation to end.
  message_loop.reset();  // Can't have 2 MessageLoops alive at once.
  message_loop.reset(new MessageLoop);
  EXPECT_FALSE(script_badge.GetIconAnimation(1));  // Sanity check.

  script_badge.SetAppearance(1, ExtensionAction::ACTIVE);
  EXPECT_FALSE(script_badge.GetIconAnimation(1))
      << "The animation should not play again if the icon was already visible.";
}

TEST(ExtensionActionTest, Icon) {
  ActionInfo action_info;
  action_info.default_icon.Add(16, "icon16.png");
  ExtensionAction page_action("", ActionInfo::TYPE_PAGE,
                              action_info);
  ASSERT_TRUE(page_action.default_icon());
  EXPECT_EQ("icon16.png",
            page_action.default_icon()->Get(
                16, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("",
            page_action.default_icon()->Get(
                17, ExtensionIconSet::MATCH_BIGGER));
}

TEST(ExtensionActionTest, Badge) {
  ExtensionAction action("", ActionInfo::TYPE_PAGE,
                         ActionInfo());
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

TEST(ExtensionActionTest, BadgeTextColor) {
  ExtensionAction action("", ActionInfo::TYPE_PAGE,
                         ActionInfo());
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

TEST(ExtensionActionTest, BadgeBackgroundColor) {
  ExtensionAction action("", ActionInfo::TYPE_PAGE,
                         ActionInfo());
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

TEST(ExtensionActionTest, PopupUrl) {
  GURL url_unset;
  GURL url_foo("http://www.example.com/foo.html");
  GURL url_bar("http://www.example.com/bar.html");
  GURL url_baz("http://www.example.com/baz.html");

  ActionInfo action_info;
  action_info.default_popup_url = url_foo;
  ExtensionAction action("", ActionInfo::TYPE_PAGE, action_info);

  ASSERT_EQ(url_foo, action.GetPopupUrl(1));
  ASSERT_EQ(url_foo, action.GetPopupUrl(100));
  ASSERT_TRUE(action.HasPopup(1));
  ASSERT_TRUE(action.HasPopup(100));

  action.SetPopupUrl(ExtensionAction::kDefaultTabId, url_unset);
  ASSERT_EQ(url_unset, action.GetPopupUrl(1));
  ASSERT_EQ(url_unset, action.GetPopupUrl(100));
  ASSERT_FALSE(action.HasPopup(1));
  ASSERT_FALSE(action.HasPopup(100));

  action.SetPopupUrl(100, url_bar);
  ASSERT_EQ(url_unset, action.GetPopupUrl(1));
  ASSERT_EQ(url_bar, action.GetPopupUrl(100));

  action.SetPopupUrl(ExtensionAction::kDefaultTabId, url_baz);
  ASSERT_EQ(url_baz, action.GetPopupUrl(1));
  ASSERT_EQ(url_bar, action.GetPopupUrl(100));

  action.ClearAllValuesForTab(100);
  ASSERT_EQ(url_baz, action.GetPopupUrl(1));
  ASSERT_EQ(url_baz, action.GetPopupUrl(100));
}

}  // namespace
