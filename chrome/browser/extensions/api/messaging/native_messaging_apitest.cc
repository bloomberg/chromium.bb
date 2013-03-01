// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature.h"

namespace {

const char kHostName[] = "com.google.chrome.test.echo";

void CreateTestManifestFile(base::FilePath manifest_path) {
  scoped_ptr<base::DictionaryValue> manifest(new base::DictionaryValue());
  manifest->SetString("name", kHostName);
  manifest->SetString("description", "Native Messaging Echo Test");
  manifest->SetString("type", "stdio");

  base::FilePath test_user_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_user_data_dir));
  test_user_data_dir = test_user_data_dir.AppendASCII("native_messaging");
  test_user_data_dir = test_user_data_dir.AppendASCII("native_hosts");
#ifdef OS_POSIX
  base::FilePath host_path = test_user_data_dir.AppendASCII("echo.py");
#else
  base::FilePath host_path = test_user_data_dir.AppendASCII("echo.bat");
#endif
  manifest->SetString("path", host_path.AsUTF8Unsafe());

  scoped_ptr<base::ListValue> origins(new base::ListValue());
  origins->AppendString("chrome-extension://knldjmfmopnpolahpmmgbagdohdnhkik/");
  manifest->Set("allowed_origins", origins.release());

  JSONFileValueSerializer serializer(manifest_path);
  ASSERT_TRUE(serializer.Serialize(*manifest));
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, NativeMessageBasic) {
  base::ScopedTempDir temp_dir;
  base::FilePath manifest_path =
      temp_dir.path().AppendASCII(std::string(kHostName) + ".json");

  ASSERT_NO_FATAL_FAILURE(CreateTestManifestFile(manifest_path));

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNativeMessaging);

  std::string hosts_option = base::StringPrintf(
      "%s=%s", kHostName, manifest_path.AsUTF8Unsafe().c_str());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kNativeMessagingHosts, hosts_option);

  ASSERT_TRUE(RunExtensionTest("native_messaging")) << message_;
}
