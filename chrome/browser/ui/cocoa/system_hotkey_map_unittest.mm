// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/system_hotkey_map.h"
#include "chrome/test/base/ui_test_utils.h"

namespace {

class SystemHotkeyMapTest : public ::testing::Test {
 public:
  SystemHotkeyMapTest() {}
};

TEST_F(SystemHotkeyMapTest, Parse) {
  std::string path = ui_test_utils::GetTestUrl(
                         base::FilePath(base::FilePath::kCurrentDirectory),
                         base::FilePath("mac/mac_system_hotkeys.plist")).path();
  NSString* file_path = [NSString stringWithUTF8String:path.c_str()];
  NSData* data = [NSData dataWithContentsOfFile:file_path];
  ASSERT_TRUE(data);

  SystemHotkeyMap map;
  bool result = map.ParseData(data);
  EXPECT_TRUE(result);

  // Command + ` is a common key binding. It should exist.
  int key_code = kVK_ANSI_Grave;
  int modifiers = NSCommandKeyMask;
  EXPECT_TRUE(map.IsHotkeyReserved(key_code, modifiers));

  // Command + Shift + ` is a common key binding. It should exist.
  modifiers = NSCommandKeyMask | NSShiftKeyMask;
  EXPECT_TRUE(map.IsHotkeyReserved(key_code, modifiers));

  // Command + Shift + Ctr + ` is not a common key binding.
  modifiers = NSCommandKeyMask | NSShiftKeyMask | NSControlKeyMask;
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, modifiers));

  // Command + L is not a common key binding.
  key_code = kVK_ANSI_L;
  modifiers = NSCommandKeyMask;
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, modifiers));
}

}  // namespace
