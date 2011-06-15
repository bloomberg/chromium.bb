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
  using VirtualKeyboardSelector::SelectVirtualKeyboardInternal;
};

TEST(VirtualKeyboardSelectorTest, TestNoKeyboard) {
  TestableVirtualKeyboardSelector selector;
  EXPECT_EQ(NULL, selector.SelectVirtualKeyboard("us"));
  EXPECT_EQ(NULL, selector.SelectVirtualKeyboard(""));
}

TEST(VirtualKeyboardSelectorTest, TestSystemKeyboard) {
  static const char* layouts[] = { "a", "b", "c" };
  VirtualKeyboard system_virtual_keyboard(
      GURL("http://system"), CreateLayoutSet(layouts), true /* is_system */);

  TestableVirtualKeyboardSelector selector;
  selector.AddVirtualKeyboard(system_virtual_keyboard.url(),
                              system_virtual_keyboard.supported_layouts(),
                              system_virtual_keyboard.is_system());

  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(system_virtual_keyboard == *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("b"));
  EXPECT_TRUE(system_virtual_keyboard == *selector.SelectVirtualKeyboard("b"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
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
  selector.AddVirtualKeyboard(system_virtual_keyboard_1.url(),
                              system_virtual_keyboard_1.supported_layouts(),
                              system_virtual_keyboard_1.is_system());
  selector.AddVirtualKeyboard(system_virtual_keyboard_2.url(),
                              system_virtual_keyboard_2.supported_layouts(),
                              system_virtual_keyboard_2.is_system());

  // At this point, system_virtual_keyboard_2 has higher priority since it's
  // added later than system_virtual_keyboard_1.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("d"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));

  // Request "b". system_virtual_keyboard_1 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("b"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("b"));

  // Now system_virtual_keyboard_1 should be selected for 'a' and 'c' since
  // it's the current virtual keyboard.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("c"));

  // Request "d" again. system_virtual_keyboard_2 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("d"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));
  // This time, system_virtual_keyboard_2 should be selected for 'a' and 'c'.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
}

TEST(VirtualKeyboardSelectorTest, TestUserKeyboard) {
  static const char* layouts[] = { "a", "b", "c" };
  VirtualKeyboard user_virtual_keyboard(
      GURL("http://user"), CreateLayoutSet(layouts), false /* is_system */);

  TestableVirtualKeyboardSelector selector;
  selector.AddVirtualKeyboard(user_virtual_keyboard.url(),
                              user_virtual_keyboard.supported_layouts(),
                              user_virtual_keyboard.is_system());
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(user_virtual_keyboard == *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("b"));
  EXPECT_TRUE(user_virtual_keyboard == *selector.SelectVirtualKeyboard("b"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
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
  selector.AddVirtualKeyboard(user_virtual_keyboard_1.url(),
                              user_virtual_keyboard_1.supported_layouts(),
                              user_virtual_keyboard_1.is_system());
  selector.AddVirtualKeyboard(user_virtual_keyboard_2.url(),
                              user_virtual_keyboard_2.supported_layouts(),
                              user_virtual_keyboard_2.is_system());

  // At this point, user_virtual_keyboard_2 has higher priority since it's
  // added later than user_virtual_keyboard_1.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("d"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));

  // Request "b". user_virtual_keyboard_1 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("b"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("b"));

  // Now user_virtual_keyboard_1 should be selected for 'a' and 'c' since
  // it's the current virtual keyboard.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("c"));

  // Request "d" again. user_virtual_keyboard_2 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("d"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));
  // This time, user_virtual_keyboard_2 should be selected for 'a' and 'c'.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
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
  selector.AddVirtualKeyboard(user_virtual_keyboard_1.url(),
                              user_virtual_keyboard_1.supported_layouts(),
                              user_virtual_keyboard_1.is_system());
  selector.AddVirtualKeyboard(user_virtual_keyboard_2.url(),
                              user_virtual_keyboard_2.supported_layouts(),
                              user_virtual_keyboard_2.is_system());
  selector.AddVirtualKeyboard(system_virtual_keyboard_1.url(),
                              system_virtual_keyboard_1.supported_layouts(),
                              system_virtual_keyboard_1.is_system());
  selector.AddVirtualKeyboard(system_virtual_keyboard_2.url(),
                              system_virtual_keyboard_2.supported_layouts(),
                              system_virtual_keyboard_2.is_system());

  // At this point, user_virtual_keyboard_2 has the highest priority.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("c"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("d"));
  EXPECT_TRUE(user_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("d"));

  // Request "b". user_virtual_keyboard_1 should be returned.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("b"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("b"));
  // Now user_virtual_keyboard_1 should be selected for 'a' and 'c' since
  // it's the current virtual keyboard.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("a"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
  EXPECT_TRUE(user_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("c"));

  // Request "x". system_virtual_keyboard_2 should be returned (since it's
  // added later than system_virtual_keyboard_1).
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("x"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("x"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("y"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("y"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("a"));

  // Switch to system_virtual_keyboard_2.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("z"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("z"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("y"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("y"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(system_virtual_keyboard_2 ==
              *selector.SelectVirtualKeyboard("a"));

  // Switch back to system_virtual_keyboard_2.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("x"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("x"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("y"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("y"));
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("a"));
  EXPECT_TRUE(system_virtual_keyboard_1 ==
              *selector.SelectVirtualKeyboard("a"));

  // Switch back to user_virtual_keyboard_1.
  ASSERT_TRUE(selector.SelectVirtualKeyboardInternal("c"));
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

}  // namespace input_method
}  // namespace chromeos
