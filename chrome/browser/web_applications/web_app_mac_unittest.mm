// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/web_app_mac.h"

#import <Cocoa/Cocoa.h>

#include <sys/xattr.h>
#include <errno.h>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/mac/app_mode_common.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

namespace {

class WebAppShortcutCreatorMock : public web_app::WebAppShortcutCreator {
 public:
  explicit WebAppShortcutCreatorMock(
      const ShellIntegration::ShortcutInfo& shortcut_info)
      : WebAppShortcutCreator(base::FilePath("/fake/path"), shortcut_info,
            UTF8ToUTF16("fake.cfbundleidentifier")) {
  }

  MOCK_CONST_METHOD0(GetDestinationPath, base::FilePath());
  MOCK_CONST_METHOD1(RevealGeneratedBundleInFinder,
                     void (const base::FilePath&));
};

ShellIntegration::ShortcutInfo GetShortcutInfo() {
  ShellIntegration::ShortcutInfo info;
  info.extension_id = "extension_id";
  info.extension_path = base::FilePath("/fake/extension/path");
  info.title = ASCIIToUTF16("Shortcut Title");
  info.url = GURL("http://example.com/");
  info.profile_path = base::FilePath("Default");
  return info;
}

}  // namespace

namespace web_app {

TEST(WebAppShortcutCreatorTest, CreateShortcut) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  ShellIntegration::ShortcutInfo info = GetShortcutInfo();

  base::FilePath dst_folder = scoped_temp_dir.path();
  base::FilePath dst_path = dst_folder.Append(UTF16ToUTF8(info.title) + ".app");

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(info);
  EXPECT_CALL(shortcut_creator, GetDestinationPath())
      .WillRepeatedly(Return(dst_folder));
  EXPECT_CALL(shortcut_creator, RevealGeneratedBundleInFinder(dst_path));

  EXPECT_TRUE(shortcut_creator.CreateShortcut());
  EXPECT_TRUE(file_util::PathExists(dst_path));

  base::FilePath plist_path = dst_path.Append("Contents").Append("Info.plist");
  NSDictionary* plist = [NSDictionary dictionaryWithContentsOfFile:
      base::mac::FilePathToNSString(plist_path)];
  EXPECT_NSEQ(base::SysUTF8ToNSString(info.extension_id),
              [plist objectForKey:app_mode::kCrAppModeShortcutIDKey]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(info.title),
              [plist objectForKey:app_mode::kCrAppModeShortcutNameKey]);
  EXPECT_NSEQ(base::SysUTF8ToNSString(info.url.spec()),
              [plist objectForKey:app_mode::kCrAppModeShortcutURLKey]);

  // Make sure all values in the plist are actually filled in.
  for (id key in plist) {
    id value = [plist valueForKey:key];
    if (!base::mac::ObjCCast<NSString>(value))
      continue;

    EXPECT_EQ([value rangeOfString:@"@APP_"].location, NSNotFound)
        << [key UTF8String] << ":" << [value UTF8String];
  }
}

TEST(WebAppShortcutCreatorTest, RunShortcut) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  ShellIntegration::ShortcutInfo info = GetShortcutInfo();

  base::FilePath dst_folder = scoped_temp_dir.path();
  base::FilePath dst_path = dst_folder.Append(UTF16ToUTF8(info.title) + ".app");

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(info);
  EXPECT_CALL(shortcut_creator, GetDestinationPath())
      .WillRepeatedly(Return(dst_folder));
  EXPECT_CALL(shortcut_creator, RevealGeneratedBundleInFinder(dst_path));

  EXPECT_TRUE(shortcut_creator.CreateShortcut());
  EXPECT_TRUE(file_util::PathExists(dst_path));

  ssize_t status = getxattr(
      dst_path.value().c_str(), "com.apple.quarantine", NULL, 0, 0, 0);
  EXPECT_EQ(-1, status);
  EXPECT_EQ(ENOATTR, errno);
}

TEST(WebAppShortcutCreatorTest, CreateFailure) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath non_existent_path =
      scoped_temp_dir.path().Append("not-existent").Append("name.app");

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(GetShortcutInfo());
  EXPECT_CALL(shortcut_creator, GetDestinationPath())
      .WillRepeatedly(Return(non_existent_path));
  EXPECT_FALSE(shortcut_creator.CreateShortcut());
}

TEST(WebAppShortcutCreatorTest, UpdateIcon) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath dst_path = scoped_temp_dir.path();

  ShellIntegration::ShortcutInfo info = GetShortcutInfo();
  info.favicon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PRODUCT_LOGO_32);
  WebAppShortcutCreatorMock shortcut_creator(info);

  shortcut_creator.UpdateIcon(dst_path);
  base::FilePath icon_path =
      dst_path.Append("Contents").Append("Resources").Append("app.icns");

  scoped_nsobject<NSImage> image([[NSImage alloc] initWithContentsOfFile:
      base::mac::FilePathToNSString(icon_path)]);
  EXPECT_TRUE(image);
  EXPECT_EQ(info.favicon.ToSkBitmap()->width(), [image size].width);
  EXPECT_EQ(info.favicon.ToSkBitmap()->height(), [image size].height);
}

}  // namespace web_app
