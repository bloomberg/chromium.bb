// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, NativeMessageBasic) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNativeMessaging);
  // Override the user data dir to point to our native app.
  extensions::Feature::ScopedCurrentChannel
      current_channel(chrome::VersionInfo::CHANNEL_DEV);
  base::FilePath test_user_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_user_data_dir));
  test_user_data_dir = test_user_data_dir.AppendASCII("native_messaging");
  ASSERT_TRUE(PathService::Override(chrome::DIR_USER_DATA, test_user_data_dir));
  ASSERT_TRUE(RunExtensionTest("native_messaging")) << message_;
}
