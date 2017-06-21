// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/shell_extension_enumerator_win.h"

#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ShellExtensionEnumeratorTest : public testing::Test {
 public:
  ShellExtensionEnumeratorTest() : shell_extension_count_(0) {}

  ~ShellExtensionEnumeratorTest() override = default;

  // Override all registry hives so that real shell extensions don't mess up
  // the unit tests.
  void OverrideRegistryHives() {
    registry_override_manager_.OverrideRegistry(HKEY_CLASSES_ROOT);
    registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
    registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE);
  }

  void OnShellExtensionEnumerated(const base::FilePath& path,
                                  uint32_t size_of_image,
                                  uint32_t time_date_stamp) {
    shell_extension_count_++;
  }

  int shell_extension_count() { return shell_extension_count_; }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  registry_util::RegistryOverrideManager registry_override_manager_;

  // Counts the number of shell extensions found via enumeration.
  int shell_extension_count_;
};

// Adds a fake shell extension entry to the registry that should be found by
// the ShellExtensionEnumerator.
void RegisterFakeShellExtension(HKEY key,
                                const wchar_t* guid,
                                const wchar_t* path) {
  base::win::RegKey class_id(
      HKEY_CLASSES_ROOT,
      base::StringPrintf(ShellExtensionEnumerator::kClassIdRegistryKeyFormat,
                         guid)
          .c_str(),
      KEY_WRITE);
  class_id.WriteValue(nullptr, path);

  base::win::RegKey registration(
      key, ShellExtensionEnumerator::kShellExtensionRegistryKey, KEY_WRITE);
  registration.WriteValue(guid, L"");
}

void OnShellExtensionPathEnumerated(
    std::vector<base::FilePath>* shell_extension_paths,
    const base::FilePath& path) {
  shell_extension_paths->push_back(path);
}

}  // namespace

// Registers a few fake shell extensions then see if
// EnumerateShellExtensionPaths() finds them.
TEST_F(ShellExtensionEnumeratorTest, EnumerateShellExtensionPaths) {
  const std::tuple<HKEY, const wchar_t*, const wchar_t*> test_cases[] = {
      {HKEY_CURRENT_USER, L"{FAKE_GUID_0001}", L"c:\\module.dll"},
      {HKEY_LOCAL_MACHINE, L"{FAKE_GUID_0002}", L"c:\\dir\\shell_ext.dll"},
      {HKEY_LOCAL_MACHINE, L"{FAKE_GUID_0003}", L"c:\\path\\test.dll"},
  };

  OverrideRegistryHives();

  // Register all fake shell extensions in test_cases.
  for (const auto& test_case : test_cases) {
    RegisterFakeShellExtension(std::get<0>(test_case), std::get<1>(test_case),
                               std::get<2>(test_case));
  }

  std::vector<base::FilePath> shell_extension_paths;
  ShellExtensionEnumerator::EnumerateShellExtensionPaths(
      base::Bind(&OnShellExtensionPathEnumerated,
                 base::Unretained(&shell_extension_paths)));

  ASSERT_EQ(3u, shell_extension_paths.size());
  for (size_t i = 0; i < arraysize(test_cases); i++) {
    // The inefficiency is fine as long as the number of test cases stay small.
    EXPECT_TRUE(base::ContainsValue(
        shell_extension_paths, base::FilePath(std::get<2>(test_cases[i]))));
  }
}

TEST_F(ShellExtensionEnumeratorTest, EnumerateShellExtensions) {
  OverrideRegistryHives();

  // Use the current exe file as an arbitrary module that exists.
  base::FilePath file_exe;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &file_exe));
  RegisterFakeShellExtension(HKEY_LOCAL_MACHINE, L"{FAKE_GUID}",
                             file_exe.value().c_str());
  EXPECT_EQ(0, shell_extension_count());

  ShellExtensionEnumerator shell_extension_enumerator(
      base::Bind(&ShellExtensionEnumeratorTest::OnShellExtensionEnumerated,
                 base::Unretained(this)));

  RunUntilIdle();

  EXPECT_EQ(1, shell_extension_count());
}
