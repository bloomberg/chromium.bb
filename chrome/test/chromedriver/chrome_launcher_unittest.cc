// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ProcessCommandLineArgs, NoArgs) {
  CommandLine command(CommandLine::NO_PROGRAM);
  base::ListValue switches;
  ASSERT_TRUE(switches.empty());
  Status status = internal::ProcessCommandLineArgs(&switches, &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(command.GetSwitches().empty());
}

TEST(ProcessCommandLineArgs, SingleArgWithoutValue) {
  CommandLine command(CommandLine::NO_PROGRAM);
  base::ListValue switches;
  switches.AppendString("enable-nacl");
  ASSERT_EQ(1u, switches.GetSize());
  Status status = internal::ProcessCommandLineArgs(&switches, &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, command.GetSwitches().size());
  ASSERT_TRUE(command.HasSwitch("enable-nacl"));
}

TEST(ProcessCommandLineArgs, SingleArgWithValue) {
  CommandLine command(CommandLine::NO_PROGRAM);
  base::ListValue switches;
  switches.AppendString("load-extension=/test/extension");
  ASSERT_EQ(1u, switches.GetSize());
  Status status = internal::ProcessCommandLineArgs(&switches, &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, command.GetSwitches().size());
  ASSERT_TRUE(command.HasSwitch("load-extension"));
  ASSERT_EQ("/test/extension", command.GetSwitchValueASCII("load-extension"));
}

TEST(ProcessCommandLineArgs, MultipleArgs) {
  CommandLine command(CommandLine::NO_PROGRAM);
  base::ListValue switches;
  switches.AppendString("disable-sync");
  switches.AppendString("user-data-dir=/test/user/data");
  ASSERT_EQ(2u, switches.GetSize());
  Status status = internal::ProcessCommandLineArgs(&switches, &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, command.GetSwitches().size());
  ASSERT_TRUE(command.HasSwitch("disable-sync"));
  ASSERT_TRUE(command.HasSwitch("user-data-dir"));
  ASSERT_EQ("/test/user/data", command.GetSwitchValueASCII("user-data-dir"));
}

TEST(ProcessExtensions, NoExtension) {
  CommandLine command(CommandLine::NO_PROGRAM);
  base::ListValue extensions;
  base::FilePath extension_dir;
  Status status = internal::ProcessExtensions(&extensions, extension_dir,
                                              &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_FALSE(command.HasSwitch("load-extension"));
}

TEST(ProcessExtensions, SingleExtension) {
  base::FilePath source_root;
  PathService::Get(base::DIR_SOURCE_ROOT, &source_root);
  base::FilePath crx_file_path = source_root.AppendASCII(
      "chrome/test/data/chromedriver/ext_test_1.crx");
  std::string crx_contents;
  ASSERT_TRUE(file_util::ReadFileToString(crx_file_path, &crx_contents));

  base::ListValue extensions;
  std::string crx_encoded;
  ASSERT_TRUE(base::Base64Encode(crx_contents, &crx_encoded));
  extensions.AppendString(crx_encoded);

  base::ScopedTempDir extension_dir;
  ASSERT_TRUE(extension_dir.CreateUniqueTempDir());

  CommandLine command(CommandLine::NO_PROGRAM);
  Status status = internal::ProcessExtensions(&extensions, extension_dir.path(),
                                              &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(command.HasSwitch("load-extension"));
  base::FilePath temp_ext_path = command.GetSwitchValuePath("load-extension");
  ASSERT_TRUE(file_util::PathExists(temp_ext_path));
}

TEST(ProcessExtensions, MultipleExtensions) {
  base::FilePath source_root;
  PathService::Get(base::DIR_SOURCE_ROOT, &source_root);
  base::FilePath test_ext_path = source_root.AppendASCII(
      "chrome/test/data/chromedriver");
  base::FilePath test_crx_1 = test_ext_path.AppendASCII("ext_test_1.crx");
  base::FilePath test_crx_2 = test_ext_path.AppendASCII("ext_test_2.crx");

  std::string crx_1_contents, crx_2_contents;
  ASSERT_TRUE(file_util::ReadFileToString(test_crx_1, &crx_1_contents));
  ASSERT_TRUE(file_util::ReadFileToString(test_crx_2, &crx_2_contents));

  base::ListValue extensions;
  std::string crx_1_encoded, crx_2_encoded;
  ASSERT_TRUE(base::Base64Encode(crx_1_contents, &crx_1_encoded));
  ASSERT_TRUE(base::Base64Encode(crx_2_contents, &crx_2_encoded));
  extensions.AppendString(crx_1_encoded);
  extensions.AppendString(crx_2_encoded);

  base::ScopedTempDir extension_dir;
  ASSERT_TRUE(extension_dir.CreateUniqueTempDir());

  CommandLine command(CommandLine::NO_PROGRAM);
  Status status = internal::ProcessExtensions(&extensions, extension_dir.path(),
                                              &command);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(command.HasSwitch("load-extension"));
  CommandLine::StringType ext_paths = command.GetSwitchValueNative(
      "load-extension");
  std::vector<CommandLine::StringType> ext_path_list;
  base::SplitString(ext_paths, FILE_PATH_LITERAL(','), &ext_path_list);
  ASSERT_EQ(2u, ext_path_list.size());
  ASSERT_TRUE(file_util::PathExists(base::FilePath(ext_path_list[0])));
  ASSERT_TRUE(file_util::PathExists(base::FilePath(ext_path_list[1])));
}

TEST(PrepareUserDataDir, CustomPrefs) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CommandLine command(CommandLine::NO_PROGRAM);
  base::DictionaryValue prefs;
  prefs.SetString("myPrefsKey", "ok");
  base::DictionaryValue local_state;
  local_state.SetString("myLocalKey", "ok");
  Status status = internal::PrepareUserDataDir(
      temp_dir.path(), &prefs, &local_state);
  ASSERT_EQ(kOk, status.code());

  base::FilePath prefs_file =
      temp_dir.path().AppendASCII("Default").AppendASCII("Preferences");
  std::string prefs_str;
  ASSERT_TRUE(file_util::ReadFileToString(prefs_file, &prefs_str));
  ASSERT_TRUE(prefs_str.find("myPrefsKey") != std::string::npos);

  base::FilePath local_state_file = temp_dir.path().AppendASCII("Local State");
  std::string local_state_str;
  ASSERT_TRUE(file_util::ReadFileToString(local_state_file, &local_state_str));
  ASSERT_TRUE(local_state_str.find("myLocalKey") != std::string::npos);
}
