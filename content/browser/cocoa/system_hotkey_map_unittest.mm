// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#import "content/browser/cocoa/system_hotkey_map.h"
#include "content/public/common/content_paths.h"

namespace content {

class SystemHotkeyMapTest : public ::testing::Test {
 public:
  SystemHotkeyMapTest() {}
};

TEST_F(SystemHotkeyMapTest, Parse) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_data_dir));

  base::FilePath test_path =
      test_data_dir.AppendASCII("mac/mac_system_hotkeys.plist");
  std::string test_path_string = test_path.AsUTF8Unsafe();
  NSString* file_path =
      [NSString stringWithUTF8String:test_path_string.c_str()];
  NSData* data = [NSData dataWithContentsOfFile:file_path];
  ASSERT_TRUE(data);

  NSDictionary* dictionary = SystemHotkeyMap::DictionaryFromData(data);
  ASSERT_TRUE(dictionary);

  SystemHotkeyMap map;
  bool result = map.ParseDictionary(dictionary);
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

TEST_F(SystemHotkeyMapTest, ParseNil) {
  NSDictionary* dictionary = nil;

  SystemHotkeyMap map;
  bool result = map.ParseDictionary(dictionary);
  EXPECT_FALSE(result);
}

}  // namespace content
