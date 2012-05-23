// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/xkeyboard.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/input_method_whitelist.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/x11_util.h"

#include <X11/Xlib.h>

using content::BrowserThread;

namespace chromeos {
namespace input_method {

namespace {

class XKeyboardTest : public testing::Test {
 public:
  XKeyboardTest()
      : util_(whitelist_.GetSupportedInputMethods()),
        ui_thread_(BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() {
    xkey_.reset(XKeyboard::Create(util_));
  }

  virtual void TearDown() {
    xkey_.reset();
  }

  InputMethodWhitelist whitelist_;
  InputMethodUtil util_;
  scoped_ptr<XKeyboard> xkey_;

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

// Returns a ModifierMap object that contains the following mapping:
// - kSearchKey is mapped to |search|.
// - kControl key is mapped to |control|.
// - kAlt key is mapped to |alt|.
ModifierMap GetMap(ModifierKey search, ModifierKey control, ModifierKey alt) {
  ModifierMap modifier_key;
  // Use the Search key as |search|.
  modifier_key.push_back(ModifierKeyPair(kSearchKey, search));
  modifier_key.push_back(ModifierKeyPair(kControlKey, control));
  modifier_key.push_back(ModifierKeyPair(kAltKey, alt));
  return modifier_key;
}

// Checks |modifier_map| and returns true if the following conditions are met:
// - kSearchKey is mapped to |search|.
// - kControl key is mapped to |control|.
// - kAlt key is mapped to |alt|.
bool CheckMap(const ModifierMap& modifier_map,
              ModifierKey search, ModifierKey control, ModifierKey alt) {
  ModifierMap::const_iterator begin = modifier_map.begin();
  ModifierMap::const_iterator end = modifier_map.end();
  if ((std::count(begin, end, ModifierKeyPair(kSearchKey, search)) == 1) &&
      (std::count(begin, end,
                  ModifierKeyPair(kControlKey, control)) == 1) &&
      (std::count(begin, end, ModifierKeyPair(kAltKey, alt)) == 1)) {
    return true;
  }
  return false;
}

// Returns true if X display is available.
bool DisplayAvailable() {
  return ui::GetXDisplay() ? true : false;
}

}  // namespace

// Tests CreateFullXkbLayoutName() function.
TEST_F(XKeyboardTest, TestCreateFullXkbLayoutNameBasic) {
  // CreateFullXkbLayoutName should not accept an empty |layout_name|.
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName(
      "", GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());

  // CreateFullXkbLayoutName should not accept an empty ModifierMap.
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName(
      "us", ModifierMap()).c_str());

  // CreateFullXkbLayoutName should not accept an incomplete ModifierMap.
  ModifierMap tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.pop_back();
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName("us", tmp_map).c_str());

  // CreateFullXkbLayoutName should not accept redundant ModifierMaps.
  tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.push_back(ModifierKeyPair(kSearchKey, kVoidKey));  // two search maps
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName("us", tmp_map).c_str());
  tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.push_back(ModifierKeyPair(kControlKey, kVoidKey));  // two ctrls
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName("us", tmp_map).c_str());
  tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.push_back(ModifierKeyPair(kAltKey, kVoidKey));  // two alts.
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName("us", tmp_map).c_str());

  // CreateFullXkbLayoutName should not accept invalid ModifierMaps.
  tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.push_back(ModifierKeyPair(kVoidKey, kSearchKey));  // can't remap void
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName("us", tmp_map).c_str());
  tmp_map = GetMap(kVoidKey, kVoidKey, kVoidKey);
  tmp_map.push_back(ModifierKeyPair(kCapsLockKey, kSearchKey));  // ditto
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName("us", tmp_map).c_str());

  // CreateFullXkbLayoutName can remap Search/Ctrl/Alt to CapsLock.
  EXPECT_STREQ("us+chromeos(capslock_disabled_disabled)",
               xkey_->CreateFullXkbLayoutName(
                   "us",
                   GetMap(kCapsLockKey, kVoidKey, kVoidKey)).c_str());
  EXPECT_STREQ("us+chromeos(disabled_capslock_disabled)",
               xkey_->CreateFullXkbLayoutName(
                   "us",
                   GetMap(kVoidKey, kCapsLockKey, kVoidKey)).c_str());
  EXPECT_STREQ("us+chromeos(disabled_disabled_capslock)",
               xkey_->CreateFullXkbLayoutName(
                   "us",
                   GetMap(kVoidKey, kVoidKey, kCapsLockKey)).c_str());

  // CreateFullXkbLayoutName should not accept non-alphanumeric characters
  // except "()-_".
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName(
      "us!", GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName(
      "us; /bin/sh", GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());
  EXPECT_STREQ("ab-c_12+chromeos(disabled_disabled_disabled)",
               xkey_->CreateFullXkbLayoutName(
                   "ab-c_12",
                   GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());

  // CreateFullXkbLayoutName should not accept upper-case ascii characters.
  EXPECT_STREQ("", xkey_->CreateFullXkbLayoutName(
      "US", GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());

  // CreateFullXkbLayoutName should accept lower-case ascii characters.
  for (int c = 'a'; c <= 'z'; ++c) {
    EXPECT_STRNE("", xkey_->CreateFullXkbLayoutName(
        std::string(3, c),
        GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());
  }

  // CreateFullXkbLayoutName should accept numbers.
  for (int c = '0'; c <= '9'; ++c) {
    EXPECT_STRNE("", xkey_->CreateFullXkbLayoutName(
        std::string(3, c),
        GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());
  }

  // CreateFullXkbLayoutName should accept a layout with a variant name.
  EXPECT_STREQ("us(dvorak)+chromeos(disabled_disabled_disabled)",
               xkey_->CreateFullXkbLayoutName(
                   "us(dvorak)",
                   GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());
  EXPECT_STREQ("jp+chromeos(disabled_disabled_disabled)",
               xkey_->CreateFullXkbLayoutName(
                   "jp",  // does not use AltGr, therefore no _keepralt.
                   GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());
}

TEST_F(XKeyboardTest, TestCreateFullXkbLayoutNameKeepCapsLock) {
  EXPECT_STREQ("us(colemak)+chromeos(search_disabled_disabled)",
               xkey_->CreateFullXkbLayoutName(
                   "us(colemak)",
                   // The 1st kVoidKey should be ignored.
                   GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());
  EXPECT_STREQ("de(neo)+chromeos(search_leftcontrol_leftcontrol_keepralt)",
               xkey_->CreateFullXkbLayoutName(
                   // The 1st kControlKey should be ignored.
                   "de(neo)", GetMap(kControlKey,
                                     kControlKey,
                                     kControlKey)).c_str());
  EXPECT_STREQ("gb(extd)+chromeos(disabled_disabled_disabled_keepralt)",
               xkey_->CreateFullXkbLayoutName(
                   "gb(extd)",
                   GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());
}

TEST_F(XKeyboardTest, TestCreateFullXkbLayoutNameKeepAlt) {
  EXPECT_STREQ("us(intl)+chromeos(disabled_disabled_disabled_keepralt)",
               xkey_->CreateFullXkbLayoutName(
                   "us(intl)", GetMap(kVoidKey, kVoidKey, kVoidKey)).c_str());
  EXPECT_STREQ("kr(kr104)+"
               "chromeos(leftcontrol_leftcontrol_leftcontrol_keepralt)",
               xkey_->CreateFullXkbLayoutName(
                   "kr(kr104)", GetMap(kControlKey,
                                       kControlKey,
                                       kControlKey)).c_str());
}

// Tests if CreateFullXkbLayoutName and ExtractLayoutNameFromFullXkbLayoutName
// functions could handle all combinations of modifier remapping.
TEST_F(XKeyboardTest, TestCreateFullXkbLayoutNameModifierKeys) {
  std::set<std::string> layouts;
  for (int i = 0; i < static_cast<int>(kNumModifierKeys); ++i) {
    for (int j = 0; j < static_cast<int>(kNumModifierKeys); ++j) {
      for (int k = 0; k < static_cast<int>(kNumModifierKeys); ++k) {
        const std::string layout = xkey_->CreateFullXkbLayoutName(
            "us", GetMap(ModifierKey(i), ModifierKey(j), ModifierKey(k)));
        // CreateFullXkbLayoutName should succeed (i.e. should not return "".)
        EXPECT_STREQ("us+", layout.substr(0, 3).c_str())
            << "layout: " << layout;
        // All 4*3*3 layouts should be different.
        EXPECT_TRUE(layouts.insert(layout).second) << "layout: " << layout;
      }
    }
  }
}

TEST_F(XKeyboardTest, TestSetCapsLockEnabled) {
  if (!DisplayAvailable()) {
    // Do not fail the test to allow developers to run unit_tests without an X
    // server (e.g. via ssh). Note that both try bots and waterfall always have
    // an X server for running browser_tests.
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  const bool initial_lock_state = xkey_->CapsLockIsEnabled();
  xkey_->SetCapsLockEnabled(true);
  EXPECT_TRUE(xkey_->CapsLockIsEnabled());
  xkey_->SetCapsLockEnabled(false);
  EXPECT_FALSE(xkey_->CapsLockIsEnabled());
  xkey_->SetCapsLockEnabled(true);
  EXPECT_TRUE(xkey_->CapsLockIsEnabled());
  xkey_->SetCapsLockEnabled(false);
  EXPECT_FALSE(xkey_->CapsLockIsEnabled());
  xkey_->SetCapsLockEnabled(initial_lock_state);
}

TEST_F(XKeyboardTest, TestSetNumLockEnabled) {
  if (!DisplayAvailable()) {
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  const unsigned int num_lock_mask = xkey_->GetNumLockMask();
  ASSERT_NE(0U, num_lock_mask);

  const bool initial_lock_state = xkey_->NumLockIsEnabled();
  xkey_->SetNumLockEnabled(true);
  EXPECT_TRUE(xkey_->NumLockIsEnabled());
  xkey_->SetNumLockEnabled(false);
  EXPECT_FALSE(xkey_->NumLockIsEnabled());
  xkey_->SetNumLockEnabled(true);
  EXPECT_TRUE(xkey_->NumLockIsEnabled());
  xkey_->SetNumLockEnabled(false);
  EXPECT_FALSE(xkey_->NumLockIsEnabled());
  xkey_->SetNumLockEnabled(initial_lock_state);
}

TEST_F(XKeyboardTest, TestSetCapsLockAndNumLockAtTheSameTime) {
  if (!DisplayAvailable()) {
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  const unsigned int num_lock_mask = xkey_->GetNumLockMask();
  ASSERT_NE(0U, num_lock_mask);

  const bool initial_caps_lock_state = xkey_->CapsLockIsEnabled();
  const bool initial_num_lock_state = xkey_->NumLockIsEnabled();

  // Flip both.
  xkey_->SetLockedModifiers(
      initial_caps_lock_state ? kDisableLock : kEnableLock,
      initial_num_lock_state ? kDisableLock : kEnableLock);
  EXPECT_EQ(!initial_caps_lock_state, xkey_->CapsLockIsEnabled());
  EXPECT_EQ(!initial_num_lock_state, xkey_->NumLockIsEnabled());

  // Flip Caps Lock.
  xkey_->SetLockedModifiers(
      initial_caps_lock_state ? kEnableLock : kDisableLock,
      kDontChange);
  // Use GetLockedModifiers() for verifying the result.
  bool c, n;
  xkey_->GetLockedModifiers(&c, &n);
  EXPECT_EQ(initial_caps_lock_state, c);
  EXPECT_EQ(!initial_num_lock_state, n);

  // Flip both.
  xkey_->SetLockedModifiers(
      initial_caps_lock_state ? kDisableLock : kEnableLock,
      initial_num_lock_state ? kEnableLock : kDisableLock);
  EXPECT_EQ(!initial_caps_lock_state, xkey_->CapsLockIsEnabled());
  EXPECT_EQ(initial_num_lock_state, xkey_->NumLockIsEnabled());

  // Flip Num Lock.
  xkey_->SetLockedModifiers(
      kDontChange,
      initial_num_lock_state ? kDisableLock : kEnableLock);
  xkey_->GetLockedModifiers(&c, &n);
  EXPECT_EQ(!initial_caps_lock_state, c);
  EXPECT_EQ(!initial_num_lock_state, n);

  // Flip both to restore the initial state.
  xkey_->SetLockedModifiers(
      initial_caps_lock_state ? kEnableLock : kDisableLock,
      initial_num_lock_state ? kEnableLock : kDisableLock);
  EXPECT_EQ(initial_caps_lock_state, xkey_->CapsLockIsEnabled());
  EXPECT_EQ(initial_num_lock_state, xkey_->NumLockIsEnabled());

  // No-op SetLockedModifiers call.
  xkey_->SetLockedModifiers(kDontChange, kDontChange);
  EXPECT_EQ(initial_caps_lock_state, xkey_->CapsLockIsEnabled());
  EXPECT_EQ(initial_num_lock_state, xkey_->NumLockIsEnabled());

  // No-op GetLockedModifiers call. Confirm it does not crash.
  xkey_->GetLockedModifiers(NULL, NULL);
}

TEST_F(XKeyboardTest, TestContainsModifierKeyAsReplacement) {
  EXPECT_FALSE(XKeyboard::ContainsModifierKeyAsReplacement(
      GetMap(kVoidKey, kVoidKey, kVoidKey), kCapsLockKey));
  EXPECT_TRUE(XKeyboard::ContainsModifierKeyAsReplacement(
      GetMap(kCapsLockKey, kVoidKey, kVoidKey), kCapsLockKey));
  EXPECT_TRUE(XKeyboard::ContainsModifierKeyAsReplacement(
      GetMap(kVoidKey, kCapsLockKey, kVoidKey), kCapsLockKey));
  EXPECT_TRUE(XKeyboard::ContainsModifierKeyAsReplacement(
      GetMap(kVoidKey, kVoidKey, kCapsLockKey), kCapsLockKey));
  EXPECT_TRUE(XKeyboard::ContainsModifierKeyAsReplacement(
      GetMap(kCapsLockKey, kCapsLockKey, kVoidKey), kCapsLockKey));
  EXPECT_TRUE(XKeyboard::ContainsModifierKeyAsReplacement(
      GetMap(kCapsLockKey, kCapsLockKey, kCapsLockKey), kCapsLockKey));
  EXPECT_TRUE(XKeyboard::ContainsModifierKeyAsReplacement(
      GetMap(kSearchKey, kVoidKey, kVoidKey), kSearchKey));
}

TEST_F(XKeyboardTest, TestSetAutoRepeatEnabled) {
  if (!DisplayAvailable()) {
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  const bool state = XKeyboard::GetAutoRepeatEnabledForTesting();
  XKeyboard::SetAutoRepeatEnabled(!state);
  EXPECT_EQ(!state, XKeyboard::GetAutoRepeatEnabledForTesting());
  // Restore the initial state.
  XKeyboard::SetAutoRepeatEnabled(state);
  EXPECT_EQ(state, XKeyboard::GetAutoRepeatEnabledForTesting());
}

TEST_F(XKeyboardTest, TestSetAutoRepeatRate) {
  if (!DisplayAvailable()) {
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  AutoRepeatRate rate;
  EXPECT_TRUE(XKeyboard::GetAutoRepeatRateForTesting(&rate));

  AutoRepeatRate tmp(rate);
  ++tmp.initial_delay_in_ms;
  ++tmp.repeat_interval_in_ms;
  EXPECT_TRUE(XKeyboard::SetAutoRepeatRate(tmp));
  EXPECT_TRUE(XKeyboard::GetAutoRepeatRateForTesting(&tmp));
  EXPECT_EQ(rate.initial_delay_in_ms + 1, tmp.initial_delay_in_ms);
  EXPECT_EQ(rate.repeat_interval_in_ms + 1, tmp.repeat_interval_in_ms);

  // Restore the initial state.
  EXPECT_TRUE(XKeyboard::SetAutoRepeatRate(rate));
  EXPECT_TRUE(XKeyboard::GetAutoRepeatRateForTesting(&tmp));
  EXPECT_EQ(rate.initial_delay_in_ms, tmp.initial_delay_in_ms);
  EXPECT_EQ(rate.repeat_interval_in_ms, tmp.repeat_interval_in_ms);
}

}  // namespace input_method
}  // namespace chromeos
