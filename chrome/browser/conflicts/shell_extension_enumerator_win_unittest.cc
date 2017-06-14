// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/shell_extension_enumerator_win.h"

#include <windows.h>

#include <iostream>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/pe_image.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestShellExtensionEnumerator : ShellExtensionEnumerator {
 public:
  using ShellExtensionEnumerator::GetModuleImageSizeAndTimeDateStamp;
};

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

// Creates a truncated copy of the current executable at |location| path to
// mimic a module with an invalid NT header.
bool CreateTruncatedModule(const base::FilePath& location) {
  base::FilePath file_exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &file_exe_path))
    return false;

  base::File file_exe(file_exe_path,
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file_exe.IsValid())
    return false;

  const size_t kSizeOfTruncatedDll = 256;
  char buffer[kSizeOfTruncatedDll];
  if (file_exe.Read(0, buffer, kSizeOfTruncatedDll) != kSizeOfTruncatedDll)
    return false;

  base::File target_file(location,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!target_file.IsValid())
    return false;

  return target_file.Write(0, buffer, kSizeOfTruncatedDll) ==
         kSizeOfTruncatedDll;
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

// Tests that GetModuleImageSizeAndTimeDateStamp() returns the same information
// from a module that has been loaded in memory.
TEST_F(ShellExtensionEnumeratorTest, GetModuleImageSizeAndTimeDateStamp) {
  // Use the current exe file as an arbitrary module that exists.
  base::FilePath file_exe;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &file_exe));

  // Read the values from the loaded module.
  base::ScopedNativeLibrary scoped_loaded_module(file_exe);
  base::win::PEImage pe_image(scoped_loaded_module.get());
  IMAGE_NT_HEADERS* nt_headers = pe_image.GetNTHeaders();

  // Read the values from the module on disk.
  uint32_t size_of_image = 0;
  uint32_t time_date_stamp = 0;
  EXPECT_TRUE(TestShellExtensionEnumerator::GetModuleImageSizeAndTimeDateStamp(
      file_exe, &size_of_image, &time_date_stamp));

  EXPECT_EQ(nt_headers->OptionalHeader.SizeOfImage, size_of_image);
  EXPECT_EQ(nt_headers->FileHeader.TimeDateStamp, time_date_stamp);
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

TEST_F(ShellExtensionEnumeratorTest, NonexistentDll) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath nonexistant_dll =
      scoped_temp_dir.GetPath().Append(L"nonexistant.dll");

  uint32_t size_of_image = 0;
  uint32_t time_date_stamp = 0;
  EXPECT_FALSE(TestShellExtensionEnumerator::GetModuleImageSizeAndTimeDateStamp(
      nonexistant_dll, &size_of_image, &time_date_stamp));
}

TEST_F(ShellExtensionEnumeratorTest, InvalidNTHeader) {
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath invalid_dll =
      scoped_temp_dir.GetPath().Append(L"truncated.dll");
  ASSERT_TRUE(CreateTruncatedModule(invalid_dll));

  uint32_t size_of_image = 0;
  uint32_t time_date_stamp = 0;
  EXPECT_FALSE(TestShellExtensionEnumerator::GetModuleImageSizeAndTimeDateStamp(
      invalid_dll, &size_of_image, &time_date_stamp));
}
