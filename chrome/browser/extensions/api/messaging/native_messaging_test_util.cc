// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#endif

namespace extensions {

namespace {

void WriteTestNativeHostManifest(base::FilePath manifest_path) {
  scoped_ptr<base::DictionaryValue> manifest(new base::DictionaryValue());
  manifest->SetString("name", ScopedTestNativeMessagingHost::kHostName);
  manifest->SetString("description", "Native Messaging Echo Test");
  manifest->SetString("type", "stdio");

  base::FilePath test_user_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_user_data_dir));
  test_user_data_dir = test_user_data_dir.AppendASCII("native_messaging");
  test_user_data_dir = test_user_data_dir.AppendASCII("native_hosts");
#if defined(OS_POSIX)
  base::FilePath host_path = test_user_data_dir.AppendASCII("echo.py");
#else
  base::FilePath host_path = test_user_data_dir.AppendASCII("echo.bat");
#endif
  manifest->SetString("path", host_path.AsUTF8Unsafe());

  scoped_ptr<base::ListValue> origins(new base::ListValue());
  origins->AppendString(base::StringPrintf(
      "chrome-extension://%s/", ScopedTestNativeMessagingHost::kExtensionId));
  manifest->Set("allowed_origins", origins.release());

  JSONFileValueSerializer serializer(manifest_path);
  ASSERT_TRUE(serializer.Serialize(*manifest));
}

}  // namespace

const char ScopedTestNativeMessagingHost::kHostName[] =
    "com.google.chrome.test.echo";
const char ScopedTestNativeMessagingHost::kExtensionId[] =
    "knldjmfmopnpolahpmmgbagdohdnhkik";

ScopedTestNativeMessagingHost::ScopedTestNativeMessagingHost() {}

void ScopedTestNativeMessagingHost::RegisterTestHost(bool user_level) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ScopedTestNativeMessagingHost test_host;
  base::FilePath manifest_path = temp_dir_.path().AppendASCII(
      std::string(ScopedTestNativeMessagingHost::kHostName) + ".json");
  ASSERT_NO_FATAL_FAILURE(WriteTestNativeHostManifest(manifest_path));

#if defined(OS_WIN)
  HKEY root_key = user_level ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  registry_override_.OverrideRegistry(root_key, L"native_messaging");
  base::string16 key = L"SOFTWARE\\Google\\Chrome\\NativeMessagingHosts\\" +
                       base::UTF8ToUTF16(kHostName);
  base::win::RegKey manifest_key(
      root_key, key.c_str(),
      KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK);
  ASSERT_EQ(ERROR_SUCCESS,
            manifest_key.WriteValue(NULL, manifest_path.value().c_str()));
#else
  path_override_.reset(new base::ScopedPathOverride(
      user_level ? chrome::DIR_USER_NATIVE_MESSAGING
                 : chrome::DIR_NATIVE_MESSAGING,
      temp_dir_.path()));
#endif
}

ScopedTestNativeMessagingHost::~ScopedTestNativeMessagingHost() {}

}  // namespace extensions
