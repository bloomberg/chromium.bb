// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

const char kTestNativeMessagingHostName[] = "com.google.chrome.test.echo";
const char kTestNativeMessagingExtensionId[] =
    "knldjmfmopnpolahpmmgbagdohdnhkik";

void CreateTestNativeHostManifest(base::FilePath manifest_path) {
  scoped_ptr<base::DictionaryValue> manifest(new base::DictionaryValue());
  manifest->SetString("name", kTestNativeMessagingHostName);
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
  origins->AppendString(base::StringPrintf("chrome-extension://%s/",
                                           kTestNativeMessagingExtensionId));
  manifest->Set("allowed_origins", origins.release());

  JSONFileValueSerializer serializer(manifest_path);
  ASSERT_TRUE(serializer.Serialize(*manifest));
}

}  // namespace extensions
