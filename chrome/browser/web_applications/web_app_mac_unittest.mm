// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_mac.h"

#include "base/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/scoped_temp_dir.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/mac/app_mode_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

namespace {

class WebAppShortcutCreatorMock : public web_app::WebAppShortcutCreator {
 public:
  explicit WebAppShortcutCreatorMock(
      const ShellIntegration::ShortcutInfo& shortcut_info)
      : WebAppShortcutCreator(shortcut_info) {
  }

  MOCK_CONST_METHOD1(GetDestinationPath, FilePath(const FilePath&));
};

ShellIntegration::ShortcutInfo GetShortcutInfo() {
  ShellIntegration::ShortcutInfo info;
  info.extension_id = "extension_id";
  info.title = ASCIIToUTF16("Shortcut Title");
  info.url = GURL("http://example.com/");
  return info;
}

// This test currently fails because the Mac app loader isn't built yet.
TEST(WebAppShortcutCreatorTest, FAILS_CreateShortcut) {
  ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  FilePath dst_path = scoped_temp_dir.path().Append("a.app");

  ShellIntegration::ShortcutInfo info = GetShortcutInfo();
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(info);
  EXPECT_CALL(shortcut_creator, GetDestinationPath(_))
      .WillRepeatedly(Return(dst_path));
  EXPECT_TRUE(shortcut_creator.CreateShortcut());
  EXPECT_TRUE(file_util::PathExists(dst_path));

  FilePath plist_path = dst_path.Append("Contents").Append("Info.plist");
  NSDictionary* plist = [NSDictionary dictionaryWithContentsOfFile:
      base::mac::FilePathToNSString(plist_path)];
  EXPECT_NSEQ(base::SysUTF8ToNSString(info.extension_id),
              [plist objectForKey:app_mode::kCrAppModeShortcutIDKey]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(info.title),
              [plist objectForKey:app_mode::kCrAppModeShortcutNameKey]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(info.url.spec()),
              [plist objectForKey:app_mode::kCrAppModeShortcutURLKey]);

  // Make sure all values in the plist are actually filled in.
  for (NSString* value in [plist allValues])
    EXPECT_FALSE([value hasPrefix:@"@APP_"]);
}

TEST(WebAppShortcutCreatorTest, CreateFailure) {
  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(GetShortcutInfo());
  EXPECT_CALL(shortcut_creator, GetDestinationPath(_))
      .WillRepeatedly(Return(FilePath("/non-existant/path/")));
  EXPECT_FALSE(shortcut_creator.CreateShortcut());
}

}  // namespace
