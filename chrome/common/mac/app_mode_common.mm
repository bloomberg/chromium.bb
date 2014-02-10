// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/app_mode_common.h"

#include "base/file_util.h"

namespace app_mode {

const char kAppShimSocketShortName[] = "Socket";
const char kAppShimSocketSymlinkName[] = "App Shim Socket";

const char kAppListModeId[] = "app_list";

const char kLaunchedByChromeProcessId[] = "launched-by-chrome-process-id";

NSString* const kCFBundleDisplayNameKey = @"CFBundleDisplayName";
NSString* const kLSHasLocalizedDisplayNameKey = @"LSHasLocalizedDisplayName";
NSString* const kBrowserBundleIDKey = @"CrBundleIdentifier";
NSString* const kCrAppModeShortcutIDKey = @"CrAppModeShortcutID";
NSString* const kCrAppModeShortcutNameKey = @"CrAppModeShortcutName";
NSString* const kCrAppModeShortcutURLKey = @"CrAppModeShortcutURL";
NSString* const kCrAppModeUserDataDirKey = @"CrAppModeUserDataDir";
NSString* const kCrAppModeProfileDirKey = @"CrAppModeProfileDir";
NSString* const kCrAppModeProfileNameKey = @"CrAppModeProfileName";

NSString* const kLastRunAppBundlePathPrefsKey = @"LastRunAppBundlePath";

NSString* const kShortcutIdPlaceholder = @"APP_MODE_SHORTCUT_ID";
NSString* const kShortcutNamePlaceholder = @"APP_MODE_SHORTCUT_NAME";
NSString* const kShortcutURLPlaceholder = @"APP_MODE_SHORTCUT_URL";
NSString* const kShortcutBrowserBundleIDPlaceholder =
                    @"APP_MODE_BROWSER_BUNDLE_ID";

ChromeAppModeInfo::ChromeAppModeInfo()
    : major_version(0),
      minor_version(0),
      argc(0),
      argv(0) {
}

ChromeAppModeInfo::~ChromeAppModeInfo() {
}

void VerifySocketPermissions(const base::FilePath& socket_path) {
  CHECK(base::PathIsWritable(socket_path));
  base::FilePath socket_dir = socket_path.DirName();
  int socket_dir_mode = 0;
  CHECK(base::GetPosixFilePermissions(socket_dir, &socket_dir_mode));
  CHECK_EQ(0700, socket_dir_mode);
}

}  // namespace app_mode
