// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/virtual_keyboard_selector.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

// For EXPECT_TRUE calls below. The operator has to be in the global namespace.
static bool operator==(const chromeos::input_method::VirtualKeyboard& lhs,
                       const chromeos::input_method::VirtualKeyboard& rhs) {
  return lhs.GetURLForLayout("") == rhs.GetURLForLayout("");
}

namespace {

template <size_t L>
std::set<std::string> CreateLayoutSet(const char* (&layouts)[L]) {
  std::set<std::string> result;
  for (size_t i = 0; i < L; ++i) {
    result.insert(layouts[i]);
  }
  return result;
}

}  // namespace

namespace chromeos {
namespace input_method {

class TestableVirtualKeyboardSelector : public VirtualKeyboardSelector {
 public:
  // Change access rights.
  using VirtualKeyboardSelector::SelectVirtualKeyboardWithoutPreferences;
  using VirtualKeyboardSelector::user_preference;
};

TEST(VirtualKeyboardSelectorTest, TestNoKeyboard) {
  TestableVirtualKeyboardSelector selector;
  EXPECT_EQ(NULL, selector.SelectVirtualKeyboard("us"));
  EXPECT_EQ(NULL, selector.SelectVirtualKeyboard(""));
}

TEST(VirtualKeyboardSelectorTest, TestAddVirtualKeyboard) {
  static const char* layouts[] = { "a", "b", "c" };

  // The first two keyboards have the same URL.
  VirtualKeyboard virtual_keyboard_1(
      GURL("http://url1"), CreateLayoutSet(layouts), true /* is_system */);
  VirtualKeyboard virtual_keyboard_2(
      GURL("http://url1"), CreateLayoutSet(layouts), false /* is_system */);
  VirtualKeyboard virtual_keyboard_3(
      GURL("http://url2"), CreateLayoutSet(layouts), false /* is_system */);

  TestableVirtualKeyboardSelector selector;
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_1.url(),
      virtual_keyboard_1.supported_layouts(),
      virtual_keyboard_1.is_system()));

  // You can't add the same keyboard twice.
  EXPECT_FALSE(selector.AddVirtualKeyboard(
      virtual_keyboard_1.url(),
      virtual_keyboard_1.supported_layouts(),
      virtual_keyboard_1.is_system()));
  EXPECT_FALSE(selector.AddVirtualKeyboard(
      virtual_keyboard_2.url(),
      virtual_keyboard_2.supported_layouts(),
      virtual_keyboard_2.is_system()));

  EXPECT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_3.url(),
      virtual_keyboard_3.supported_layouts(),
      virtual_keyboard_3.is_system()));
}

TEST(VirtualKeyboardSelectorTest, TestSystemKeyboard) {
  static const char* layouts[] = { "a", "b", "c" };
  VirtualKeyboard system_virtual_keyboard(
      GURL("http://system"), CreateLayoutSet(layouts), true /* is_system */);

  TestableVirtualKeyboardSelector selector;
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      system_virtual_keyboard.url(),
      system_virtual_keyboard.supported_layouts(),
      system_virtual_keyboard.is_system()));

  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(system_virtual_keyboard == *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("b"));
  EXPECT_TRUE(system_virtual_keyboard == *selector.SelectVirtualKeyboard("b"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(system_virtual_keyboard == *selector.SelectVirtualKeyboard("c"));
  EXPECT_EQ(NULL, selector.SelectVirtualKeyboard("d"));
  EXPECT_EQ(NULL, selector.SelectVirtualKeyboard("aa"));
  EXPECT_EQ(NULL, selector.SelectVirtualKeyboard(""));
}

TEST(VirtualKeyboardSelectorTest, TestTwoSystemKeyboards) {
  static const char* layouts_1[] = { "a", "b", "c" };
  static const char* layouts_2[] = { "a", "c", "d" };

  VirtualKeyboard system_virtual_keyboard_1(
      GURL("http://system1"), CreateLayoutSet(layouts_1), true /* is_system */);
  VirtualKeyboard system_virtual_keyboard_2(
      GURL("http://system2"), CreateLayoutSet(layouts_2), true /* is_system */);

  TestableVirtualKeyboardSelector selector;
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      system_virtual_keyboard_1.url(),
      system_virtual_keyboard_1.supported_layouts(),
      system_virtual_keyboard_1.is_system()));
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      system_virtual_keyboard_2.url(),
      system_virtual_keyboard_2.supported_layouts(),
      system_virtual_keyboard_2.is_system()));

  // At this point, system_virtual_keyboard_2 has higher priority since it's
  // added later than system_virtual_keyboard_1.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("d"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));

  // Request "b". system_virtual_keyboard_1 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("b"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("b"));

  // Now system_virtual_keyboard_1 should be selected for 'a' and 'c' since
  // it's the current virtual keyboard.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("c"));

  // Request "d" again. system_virtual_keyboard_2 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("d"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));
  // This time, system_virtual_keyboard_2 should be selected for 'a' and 'c'.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
}

TEST(VirtualKeyboardSelectorTest, TestUserKeyboard) {
  static const char* layouts[] = { "a", "b", "c" };
  VirtualKeyboard user_virtual_keyboard(
      GURL("http://user"), CreateLayoutSet(layouts), false /* is_system */);

  TestableVirtualKeyboardSelector selector;
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      user_virtual_keyboard.url(),
      user_virtual_keyboard.supported_layouts(),
      user_virtual_keyboard.is_system()));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(user_virtual_keyboard == *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("b"));
  EXPECT_TRUE(user_virtual_keyboard == *selector.SelectVirtualKeyboard("b"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(user_virtual_keyboard == *selector.SelectVirtualKeyboard("c"));
  EXPECT_EQ(NULL, selector.SelectVirtualKeyboard("d"));
  EXPECT_EQ(NULL, selector.SelectVirtualKeyboard("aa"));
  EXPECT_EQ(NULL, selector.SelectVirtualKeyboard(""));
}

TEST(VirtualKeyboardSelectorTest, TestTwoUserKeyboards) {
  static const char* layouts_1[] = { "a", "b", "c" };
  static const char* layouts_2[] = { "a", "c", "d" };

  VirtualKeyboard user_virtual_keyboard_1(
      GURL("http://user1"), CreateLayoutSet(layouts_1), false /* is_system */);
  VirtualKeyboard user_virtual_keyboard_2(
      GURL("http://user2"), CreateLayoutSet(layouts_2), false /* is_system */);

  TestableVirtualKeyboardSelector selector;
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      user_virtual_keyboard_1.url(),
      user_virtual_keyboard_1.supported_layouts(),
      user_virtual_keyboard_1.is_system()));
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      user_virtual_keyboard_2.url(),
      user_virtual_keyboard_2.supported_layouts(),
      user_virtual_keyboard_2.is_system()));

  // At this point, user_virtual_keyboard_2 has higher priority since it's
  // added later than user_virtual_keyboard_1.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("d"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));

  // Request "b". user_virtual_keyboard_1 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("b"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("b"));

  // Now user_virtual_keyboard_1 should be selected for 'a' and 'c' since
  // it's the current virtual keyboard.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("c"));

  // Request "d" again. user_virtual_keyboard_2 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("d"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));
  // This time, user_virtual_keyboard_2 should be selected for 'a' and 'c'.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
}

TEST(VirtualKeyboardSelectorTest, TestUserSystemMixed) {
  static const char* ulayouts_1[] = { "a", "b", "c" };
  static const char* ulayouts_2[] = { "a", "c", "d" };
  static const char* layouts_1[] = { "a", "x", "y" };
  static const char* layouts_2[] = { "a", "y", "z" };

  VirtualKeyboard user_virtual_keyboard_1(
      GURL("http://user1"), CreateLayoutSet(ulayouts_1), false /* is_system */);
  VirtualKeyboard user_virtual_keyboard_2(
      GURL("http://user2"), CreateLayoutSet(ulayouts_2), false /* is_system */);
  VirtualKeyboard system_virtual_keyboard_1(
      GURL("http://system1"), CreateLayoutSet(layouts_1), true /* is_system */);
  VirtualKeyboard system_virtual_keyboard_2(
      GURL("http://system2"), CreateLayoutSet(layouts_2), true /* is_system */);

  TestableVirtualKeyboardSelector selector;
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      user_virtual_keyboard_1.url(),
      user_virtual_keyboard_1.supported_layouts(),
      user_virtual_keyboard_1.is_system()));
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      user_virtual_keyboard_2.url(),
      user_virtual_keyboard_2.supported_layouts(),
      user_virtual_keyboard_2.is_system()));
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      system_virtual_keyboard_1.url(),
      system_virtual_keyboard_1.supported_layouts(),
      system_virtual_keyboard_1.is_system()));
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      system_virtual_keyboard_2.url(),
      system_virtual_keyboard_2.supported_layouts(),
      system_virtual_keyboard_2.is_system()));

  // At this point, user_virtual_keyboard_2 has the highest priority.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("d"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));

  // Request "b". user_virtual_keyboard_1 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("b"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("b"));
  // Now user_virtual_keyboard_1 should be selected for 'a' and 'c' since
  // it's the current virtual keyboard.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("c"));

  // Request "x". system_virtual_keyboard_2 should be returned (since it's
  // added later than system_virtual_keyboard_1).
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("x"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("x"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("y"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("y"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("a"));

  // Switch to system_virtual_keyboard_2.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("z"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("z"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("y"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("y"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));

  // Switch back to system_virtual_keyboard_2.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("x"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("x"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("y"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("y"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("a"));

  // Switch back to user_virtual_keyboard_1.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
}

TEST(VirtualKeyboardTest, TestUrl) {
  static const char* layouts[] = { "a", "b", "c" };
  VirtualKeyboard system_virtual_keyboard(
      GURL("http://system"), CreateLayoutSet(layouts), true);

  EXPECT_EQ("http://system/index.html#a",
            system_virtual_keyboard.GetURLForLayout("a").spec());
  EXPECT_EQ("http://system/index.html#b",
            system_virtual_keyboard.GetURLForLayout("b").spec());
  EXPECT_EQ("http://system/index.html#c",
            system_virtual_keyboard.GetURLForLayout("c").spec());
  EXPECT_EQ("http://system/index.html#not-supported",
            system_virtual_keyboard.GetURLForLayout("not-supported").spec());
  EXPECT_EQ("http://system/index.html#not(supported)",
            system_virtual_keyboard.GetURLForLayout("not(supported)").spec());
  EXPECT_EQ("http://system/",
            system_virtual_keyboard.GetURLForLayout("").spec());
}

TEST(VirtualKeyboardSelectorTest, TestSetUserPreference1) {
  static const char* layouts[] = { "a", "b", "c" };

  VirtualKeyboard user_virtual_keyboard(
      GURL("http://user"), CreateLayoutSet(layouts), false /* is_system */);

  TestableVirtualKeyboardSelector selector;
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      user_virtual_keyboard.url(),
      user_virtual_keyboard.supported_layouts(),
      user_virtual_keyboard.is_system()));

  EXPECT_EQ(0U, selector.user_preference().size());
  EXPECT_FALSE(selector.SetUserPreference("bad_layout", GURL("http://user")));
  EXPECT_EQ(0U, selector.user_preference().size());
  EXPECT_FALSE(selector.SetUserPreference("a", GURL("http://bad_url")));
  EXPECT_EQ(0U, selector.user_preference().size());
  EXPECT_TRUE(selector.SetUserPreference("a", GURL("http://user")));
  EXPECT_EQ(1U, selector.user_preference().size());
  EXPECT_TRUE(selector.SetUserPreference("b", GURL("http://user")));
  EXPECT_EQ(2U, selector.user_preference().size());
  EXPECT_TRUE(selector.SetUserPreference("c", GURL("http://user")));
  EXPECT_EQ(3U, selector.user_preference().size());
}

TEST(VirtualKeyboardSelectorTest, TestSetUserPreference2) {
  static const char* layouts[] = { "a", "b", "c" };

  VirtualKeyboard system_virtual_keyboard(
      GURL("http://system"), CreateLayoutSet(layouts), true /* is_system */);

  TestableVirtualKeyboardSelector selector;
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      system_virtual_keyboard.url(),
      system_virtual_keyboard.supported_layouts(),
      system_virtual_keyboard.is_system()));

  EXPECT_EQ(0U, selector.user_preference().size());
  EXPECT_FALSE(selector.SetUserPreference("bad_layout", GURL("http://system")));
  EXPECT_EQ(0U, selector.user_preference().size());
  EXPECT_FALSE(selector.SetUserPreference("a", GURL("http://bad_url")));
  EXPECT_EQ(0U, selector.user_preference().size());
  EXPECT_TRUE(selector.SetUserPreference("a", GURL("http://system")));
  EXPECT_EQ(1U, selector.user_preference().size());
  EXPECT_TRUE(selector.SetUserPreference("b", GURL("http://system")));
  EXPECT_EQ(2U, selector.user_preference().size());
  EXPECT_TRUE(selector.SetUserPreference("c", GURL("http://system")));
  EXPECT_EQ(3U, selector.user_preference().size());
}

TEST(VirtualKeyboardSelectorTest, TestRemoveUserPreference) {
  static const char* layouts[] = { "a", "b", "c" };

  VirtualKeyboard user_virtual_keyboard_1(
      GURL("http://user1"), CreateLayoutSet(layouts), false /* is_system */);
  VirtualKeyboard user_virtual_keyboard_2(
      GURL("http://user2"), CreateLayoutSet(layouts), false /* is_system */);

  TestableVirtualKeyboardSelector selector;
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      user_virtual_keyboard_1.url(),
      user_virtual_keyboard_1.supported_layouts(),
      user_virtual_keyboard_1.is_system()));
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      user_virtual_keyboard_2.url(),
      user_virtual_keyboard_2.supported_layouts(),
      user_virtual_keyboard_2.is_system()));

  EXPECT_TRUE(selector.SetUserPreference("a", GURL("http://user1")));
  EXPECT_TRUE(selector.SetUserPreference("b", GURL("http://user1")));
  EXPECT_TRUE(selector.SetUserPreference("c", GURL("http://user1")));
  EXPECT_EQ(3U, selector.user_preference().size());

  selector.RemoveUserPreference("b");
  EXPECT_EQ(2U, selector.user_preference().size());
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("b"));
  // user_virtual_keyboard_2 should be selected here since the keyboard is
  // added most recently and the user preference on "b" is already removed.
  EXPECT_TRUE(user_virtual_keyboard_2 == *selector.SelectVirtualKeyboard("b"));

  selector.ClearAllUserPreferences();
  EXPECT_EQ(0U, selector.user_preference().size());
}

TEST(VirtualKeyboardSelectorTest, TestSetUserPreferenceUserSystemMixed) {
  static const char* ulayouts_1[] = { "a", "b", "c" };
  static const char* ulayouts_2[] = { "a", "c", "d" };
  static const char* layouts_1[] = { "a", "x", "y" };
  static const char* layouts_2[] = { "a", "y", "z" };

  VirtualKeyboard user_virtual_keyboard_1(
      GURL("http://user1"), CreateLayoutSet(ulayouts_1), false /* is_system */);
  VirtualKeyboard user_virtual_keyboard_2(
      GURL("http://user2"), CreateLayoutSet(ulayouts_2), false /* is_system */);
  VirtualKeyboard system_virtual_keyboard_1(
      GURL("http://system1"), CreateLayoutSet(layouts_1), true /* is_system */);
  VirtualKeyboard system_virtual_keyboard_2(
      GURL("http://system2"), CreateLayoutSet(layouts_2), true /* is_system */);

  TestableVirtualKeyboardSelector selector;
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      user_virtual_keyboard_1.url(),
      user_virtual_keyboard_1.supported_layouts(),
      user_virtual_keyboard_1.is_system()));
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      user_virtual_keyboard_2.url(),
      user_virtual_keyboard_2.supported_layouts(),
      user_virtual_keyboard_2.is_system()));
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      system_virtual_keyboard_1.url(),
      system_virtual_keyboard_1.supported_layouts(),
      system_virtual_keyboard_1.is_system()));
  EXPECT_TRUE(selector.AddVirtualKeyboard(
      system_virtual_keyboard_2.url(),
      system_virtual_keyboard_2.supported_layouts(),
      system_virtual_keyboard_2.is_system()));

  // Set and then remove user prefs (=NOP).
  EXPECT_TRUE(selector.SetUserPreference("a", GURL("http://system1")));
  EXPECT_TRUE(selector.SetUserPreference("z", GURL("http://system2")));
  selector.ClearAllUserPreferences();

  // At this point, user_virtual_keyboard_2 has the highest priority.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("d"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));

  // Request "b". user_virtual_keyboard_1 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("b"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("b"));

  // Set user pref.
  EXPECT_TRUE(selector.SetUserPreference("a", GURL("http://user2")));

  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==  // follow the user pref.
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));

  // Request "x". system_virtual_keyboard_2 should be returned (since it's
  // added later than system_virtual_keyboard_1).
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("x"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("x"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("y"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("y"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==  // follow the user pref.
              *selector.SelectVirtualKeyboard("a"));

  // Switch to system_virtual_keyboard_2.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("z"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("z"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("y"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("y"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==  // follow the user pref.
              *selector.SelectVirtualKeyboard("a"));

  // Switch back to system_virtual_keyboard_2.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("x"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("x"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("y"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("y"));

  // Remove it.
  selector.RemoveUserPreference("a");

  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("a"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==   // user pref is no longer available
              *selector.SelectVirtualKeyboard("a"));

  // Switch back to user_virtual_keyboard_1.
  ASSERT_TRUE(selector.SelectVirtualKeyboardWithoutPreferences("c"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
}

}  // namespace input_method
}  // namespace chromeos
