// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, NativeMessageBasic) {
  base::ScopedTempDir temp_dir;
  base::FilePath manifest_path = temp_dir.path().AppendASCII(
      std::string(extensions::kTestNativeMessagingHostName) + ".json");
  ASSERT_NO_FATAL_FAILURE(
      extensions::CreateTestNativeHostManifest(manifest_path));

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNativeMessaging);

  std::string hosts_option = base::StringPrintf(
      "%s=%s", extensions::kTestNativeMessagingHostName,
      manifest_path.AsUTF8Unsafe().c_str());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kNativeMessagingHosts, hosts_option);

  ASSERT_TRUE(RunExtensionTest("native_messaging")) << message_;
}
