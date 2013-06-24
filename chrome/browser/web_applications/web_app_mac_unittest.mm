// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/web_applications/web_app_mac.h"

#import <Cocoa/Cocoa.h>

#include <errno.h>
#include <sys/xattr.h>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
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

const char kFakeChromeBundleId[] = "fake.cfbundleidentifier";

class WebAppShortcutCreatorMock : public web_app::WebAppShortcutCreator {
 public:
  explicit WebAppShortcutCreatorMock(
      const base::FilePath& app_data_path,
      const ShellIntegration::ShortcutInfo& shortcut_info)
      : WebAppShortcutCreator(app_data_path,
                              shortcut_info,
                              kFakeChromeBundleId) {
  }

  MOCK_CONST_METHOD0(GetDestinationPath, base::FilePath());
  MOCK_CONST_METHOD1(GetAppBundleById,
                     base::FilePath(const std::string& bundle_id));
  MOCK_CONST_METHOD0(RevealAppShimInFinder, void());
};

ShellIntegration::ShortcutInfo GetShortcutInfo() {
  ShellIntegration::ShortcutInfo info;
  info.extension_id = "extensionid";
  info.extension_path = base::FilePath("/fake/extension/path");
  info.title = ASCIIToUTF16("Shortcut Title");
  info.url = GURL("http://example.com/");
  info.profile_path = base::FilePath("Profile 1");
  info.profile_name = "profile name";
  return info;
}

}  // namespace

namespace web_app {

TEST(WebAppShortcutCreatorTest, CreateShortcuts) {
  base::ScopedTempDir temp_app_data_path;
  EXPECT_TRUE(temp_app_data_path.CreateUniqueTempDir());
  base::ScopedTempDir temp_dst_dir;
  EXPECT_TRUE(temp_dst_dir.CreateUniqueTempDir());

  ShellIntegration::ShortcutInfo info = GetShortcutInfo();

  base::FilePath app_name(
      info.profile_path.value() + " " + info.extension_id + ".app");
  base::FilePath app_in_app_data_path_path =
      temp_app_data_path.path().Append(app_name);
  base::FilePath dst_folder = temp_dst_dir.path();
  base::FilePath dst_path = dst_folder.Append(app_name);

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(
      temp_app_data_path.path(), info);
  EXPECT_CALL(shortcut_creator, GetDestinationPath())
      .WillRepeatedly(Return(dst_folder));
  EXPECT_CALL(shortcut_creator, RevealAppShimInFinder());

  EXPECT_TRUE(shortcut_creator.CreateShortcuts());
  EXPECT_TRUE(file_util::PathExists(app_in_app_data_path_path));
  EXPECT_TRUE(file_util::PathExists(dst_path));
  EXPECT_EQ(dst_path.BaseName(), shortcut_creator.GetShortcutName());

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

TEST(WebAppShortcutCreatorTest, UpdateShortcuts) {
  base::ScopedTempDir temp_app_data_path;
  EXPECT_TRUE(temp_app_data_path.CreateUniqueTempDir());
  base::ScopedTempDir temp_dst_dir;
  EXPECT_TRUE(temp_dst_dir.CreateUniqueTempDir());
  base::ScopedTempDir temp_dst_dir_other;
  EXPECT_TRUE(temp_dst_dir_other.CreateUniqueTempDir());

  ShellIntegration::ShortcutInfo info = GetShortcutInfo();

  base::FilePath app_name(
      info.profile_path.value() + " " + info.extension_id + ".app");
  base::FilePath app_in_app_data_path_path =
      temp_app_data_path.path().Append(app_name);
  base::FilePath dst_folder = temp_dst_dir.path();
  base::FilePath other_folder = temp_dst_dir_other.path();

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(
      temp_app_data_path.path(), info);
  EXPECT_CALL(shortcut_creator, GetDestinationPath())
      .WillRepeatedly(Return(dst_folder));

  std::string expected_bundle_id = kFakeChromeBundleId;
  expected_bundle_id += ".app.Profile-1-" + info.extension_id;
  EXPECT_CALL(shortcut_creator, GetAppBundleById(expected_bundle_id))
      .WillOnce(Return(other_folder.Append(app_name)));

  shortcut_creator.BuildShortcut(other_folder.Append(app_name));

  EXPECT_TRUE(file_util::Delete(
      other_folder.Append(app_name).Append("Contents"), true));

  EXPECT_TRUE(shortcut_creator.UpdateShortcuts());
  EXPECT_FALSE(file_util::PathExists(dst_folder.Append(app_name)));
  EXPECT_TRUE(file_util::PathExists(
      other_folder.Append(app_name).Append("Contents")));

  // Also test case where GetAppBundleById fails.
  EXPECT_CALL(shortcut_creator, GetAppBundleById(expected_bundle_id))
      .WillOnce(Return(base::FilePath()));

  shortcut_creator.BuildShortcut(other_folder.Append(app_name));

  EXPECT_TRUE(file_util::Delete(
      other_folder.Append(app_name).Append("Contents"), true));

  EXPECT_FALSE(shortcut_creator.UpdateShortcuts());
  EXPECT_FALSE(file_util::PathExists(dst_folder.Append(app_name)));
  EXPECT_FALSE(file_util::PathExists(
      other_folder.Append(app_name).Append("Contents")));
}

TEST(WebAppShortcutCreatorTest, CreateAppListShortcut) {
  base::ScopedTempDir temp_dst_dir;
  EXPECT_TRUE(temp_dst_dir.CreateUniqueTempDir());

  ShellIntegration::ShortcutInfo info = GetShortcutInfo();

  base::FilePath dst_folder = temp_dst_dir.path();

  // With an empty |profile_name|, the shortcut path should not have the profile
  // directory prepended to the extension id on the app bundle name.
  info.profile_name.clear();
  base::FilePath dst_path = dst_folder.Append(info.extension_id + ".app");

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(base::FilePath(), info);
  EXPECT_CALL(shortcut_creator, GetDestinationPath())
      .WillRepeatedly(Return(dst_folder));
  EXPECT_EQ(dst_path.BaseName(), shortcut_creator.GetShortcutName());
}

TEST(WebAppShortcutCreatorTest, RunShortcut) {
  base::ScopedTempDir temp_app_data_path;
  EXPECT_TRUE(temp_app_data_path.CreateUniqueTempDir());
  base::ScopedTempDir temp_dst_dir;
  EXPECT_TRUE(temp_dst_dir.CreateUniqueTempDir());

  ShellIntegration::ShortcutInfo info = GetShortcutInfo();

  base::FilePath dst_folder = temp_dst_dir.path();
  base::FilePath dst_path = dst_folder.Append(
      info.profile_path.value() + " " + info.extension_id + ".app");

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(
      temp_app_data_path.path(), info);
  EXPECT_CALL(shortcut_creator, GetDestinationPath())
      .WillRepeatedly(Return(dst_folder));
  EXPECT_CALL(shortcut_creator, RevealAppShimInFinder());

  EXPECT_TRUE(shortcut_creator.CreateShortcuts());
  EXPECT_TRUE(file_util::PathExists(dst_path));

  ssize_t status = getxattr(
      dst_path.value().c_str(), "com.apple.quarantine", NULL, 0, 0, 0);
  EXPECT_EQ(-1, status);
  EXPECT_EQ(ENOATTR, errno);
}

TEST(WebAppShortcutCreatorTest, CreateFailure) {
  base::ScopedTempDir temp_app_data_path;
  EXPECT_TRUE(temp_app_data_path.CreateUniqueTempDir());
  base::ScopedTempDir temp_dst_dir;
  EXPECT_TRUE(temp_dst_dir.CreateUniqueTempDir());

  base::FilePath non_existent_path =
      temp_dst_dir.path().Append("not-existent").Append("name.app");

  NiceMock<WebAppShortcutCreatorMock> shortcut_creator(
      temp_app_data_path.path(), GetShortcutInfo());
  EXPECT_CALL(shortcut_creator, GetDestinationPath())
      .WillRepeatedly(Return(non_existent_path));
  EXPECT_FALSE(shortcut_creator.CreateShortcuts());
}

TEST(WebAppShortcutCreatorTest, UpdateIcon) {
  base::ScopedTempDir temp_app_data_path;
  EXPECT_TRUE(temp_app_data_path.CreateUniqueTempDir());
  base::ScopedTempDir temp_dst_dir;
  EXPECT_TRUE(temp_dst_dir.CreateUniqueTempDir());
  base::FilePath dst_path = temp_dst_dir.path();

  ShellIntegration::ShortcutInfo info = GetShortcutInfo();
  gfx::Image product_logo =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_PRODUCT_LOGO_32);
  info.favicon.Add(product_logo);
  WebAppShortcutCreatorMock shortcut_creator(temp_app_data_path.path(), info);

  ASSERT_TRUE(shortcut_creator.UpdateIcon(dst_path));
  base::FilePath icon_path =
      dst_path.Append("Contents").Append("Resources").Append("app.icns");

  base::scoped_nsobject<NSImage> image([[NSImage alloc]
      initWithContentsOfFile:base::mac::FilePathToNSString(icon_path)]);
  EXPECT_TRUE(image);
  EXPECT_EQ(product_logo.Width(), [image size].width);
  EXPECT_EQ(product_logo.Height(), [image size].height);
}

}  // namespace web_app
