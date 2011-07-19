// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A test that exercises Chrome Frame's DLL Redirctor update code. This test
// generates a new version of CF from the one already in the build folder and
// then loads them both into the current process to verify the handoff.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/scoped_temp_dir.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "base/version.h"
#include "base/win/scoped_comptr.h"
#include "chrome/installer/test/alternate_version_generator.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome_frame/test_utils.h"
#include "chrome_frame/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome_tab.h"  // NOLINT

namespace {
const wchar_t kSharedMemoryPrefix[] = L"ChromeFrameVersionBeacon_";
const uint32 kSharedMemoryBytes = 128;
}

class DllRedirectorLoadingTest : public testing::Test {
 public:
  // This sets up the following directory structure:
  //  TEMP\<version X>\npchrome_frame.dll
  //      \<version X+1>\npchrome_frame.dll
  //
  // This structure emulates enough of the directory structure of a Chrome
  // install to test upgrades.
  static void SetUpTestCase() {
    // First ensure that we can find the built Chrome Frame DLL.
    FilePath build_chrome_frame_dll = GetChromeFrameBuildPath();
    ASSERT_TRUE(file_util::PathExists(build_chrome_frame_dll));

    // Then grab its version.
    scoped_ptr<FileVersionInfo> original_version_info(
        FileVersionInfo::CreateFileVersionInfo(build_chrome_frame_dll));
    ASSERT_TRUE(original_version_info != NULL);
    original_version_.reset(Version::GetVersionFromString(
        WideToASCII(original_version_info->file_version())));
    ASSERT_TRUE(original_version_ != NULL);

    // Make a place for us to run the test from.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Make a versioned dir for the original chrome frame dll to run under.
    FilePath original_version_dir(
        temp_dir_.path().AppendASCII(original_version_->GetString()));
    ASSERT_TRUE(file_util::CreateDirectory(original_version_dir));

    // Now move the original DLL that we will operate on into a named-version
    // folder.
    original_chrome_frame_dll_ =
        original_version_dir.Append(build_chrome_frame_dll.BaseName());
    ASSERT_TRUE(file_util::CopyFile(build_chrome_frame_dll,
                                    original_chrome_frame_dll_));
    ASSERT_TRUE(file_util::PathExists(original_chrome_frame_dll_));

    // Temporary location for the new Chrome Frame DLL.
    FilePath temporary_new_chrome_frame_dll(
        temp_dir_.path().Append(build_chrome_frame_dll.BaseName()));

    // Generate the version-bumped Chrome Frame DLL to a temporary path.
    ASSERT_TRUE(
        upgrade_test::GenerateAlternatePEFileVersion(
            original_chrome_frame_dll_,
            temporary_new_chrome_frame_dll,
            upgrade_test::NEXT_VERSION));

    // Ensure it got created and grab its version.
    scoped_ptr<FileVersionInfo> new_version_info(
        FileVersionInfo::CreateFileVersionInfo(temporary_new_chrome_frame_dll));
    ASSERT_TRUE(new_version_info != NULL);
    new_version_.reset(Version::GetVersionFromString(
        WideToASCII(new_version_info->file_version())));
    ASSERT_TRUE(new_version_ != NULL);

    // Make sure the new version is larger than the old.
    ASSERT_EQ(new_version_->CompareTo(*original_version_.get()), 1);

    // Now move the new Chrome Frame dll to its final resting place:
    FilePath new_version_dir(
        temp_dir_.path().AppendASCII(new_version_->GetString()));
    ASSERT_TRUE(file_util::CreateDirectory(new_version_dir));
    new_chrome_frame_dll_ =
        new_version_dir.Append(build_chrome_frame_dll.BaseName());
    ASSERT_TRUE(file_util::Move(temporary_new_chrome_frame_dll,
                                new_chrome_frame_dll_));
    ASSERT_TRUE(file_util::PathExists(new_chrome_frame_dll_));
  }

  static void TearDownTestCase() {
    if (!temp_dir_.Delete()) {
      // The temp_dir cleanup has been observed to fail in some cases. It looks
      // like something is holding on to the Chrome Frame DLLs after they have
      // been explicitly unloaded. At least schedule them for cleanup on reboot.
      ScheduleDirectoryForDeletion(temp_dir_.path().value().c_str());
    }
  }

 protected:
  static FilePath original_chrome_frame_dll_;
  static FilePath new_chrome_frame_dll_;
  static scoped_ptr<Version> original_version_;
  static scoped_ptr<Version> new_version_;

  static ScopedTempDir temp_dir_;
};  // class DllRedirectorLoadingTest

FilePath DllRedirectorLoadingTest::original_chrome_frame_dll_;
FilePath DllRedirectorLoadingTest::new_chrome_frame_dll_;
scoped_ptr<Version> DllRedirectorLoadingTest::original_version_;
scoped_ptr<Version> DllRedirectorLoadingTest::new_version_;
ScopedTempDir DllRedirectorLoadingTest::temp_dir_;

TEST_F(DllRedirectorLoadingTest, TestDllRedirection) {
  struct TestData {
    FilePath first_dll;
    FilePath second_dll;
    Version* expected_beacon_version;
  } test_data[] = {
      {
          original_chrome_frame_dll_,
          new_chrome_frame_dll_,
          original_version_.get()
      },
      {
          new_chrome_frame_dll_,
          original_chrome_frame_dll_,
          new_version_.get()
      }
  };

  for (int i = 0; i < arraysize(test_data); ++i) {
    // First load the original dll into our test process.
    base::ScopedNativeLibrary original_library(test_data[i].first_dll);

    ASSERT_TRUE(original_library.is_valid());

    // Now query the original dll for its DllGetClassObject method and use that
    // to get the class factory for a known CLSID.
    LPFNGETCLASSOBJECT original_dgco_ptr =
        reinterpret_cast<LPFNGETCLASSOBJECT>(
            original_library.GetFunctionPointer("DllGetClassObject"));

    ASSERT_TRUE(original_dgco_ptr != NULL);

    base::win::ScopedComPtr<IClassFactory> original_class_factory;
    HRESULT hr = original_dgco_ptr(
        CLSID_ChromeFrame,
        IID_IClassFactory,
        reinterpret_cast<void**>(original_class_factory.Receive()));

    ASSERT_HRESULT_SUCCEEDED(hr);

    // Now load the new dll into our test process.
    base::ScopedNativeLibrary new_library(test_data[i].second_dll);

    ASSERT_TRUE(new_library.is_valid());

    // Now query the new dll for its DllGetClassObject method and use that
    // to get the class factory for a known CLSID.
    LPFNGETCLASSOBJECT new_dgco_ptr =
        reinterpret_cast<LPFNGETCLASSOBJECT>(
            new_library.GetFunctionPointer("DllGetClassObject"));

    ASSERT_TRUE(new_dgco_ptr != NULL);

    base::win::ScopedComPtr<IClassFactory> new_class_factory;
    hr = new_dgco_ptr(CLSID_ChromeFrame,
                      IID_IClassFactory,
                      reinterpret_cast<void**>(new_class_factory.Receive()));

    ASSERT_HRESULT_SUCCEEDED(hr);

    base::win::ScopedComPtr<IUnknown> qi_test;
    EXPECT_HRESULT_SUCCEEDED(original_class_factory.QueryInterface(IID_IUnknown,
          reinterpret_cast<void**>(qi_test.Receive())));

    EXPECT_TRUE(new_class_factory.IsSameObject(qi_test));

    // Check the version beacon.
    std::wstring beacon_name(kSharedMemoryPrefix);
    beacon_name += GetHostProcessName(false /* without extension */);
    base::SharedMemory beacon(beacon_name);

    EXPECT_TRUE(beacon.Open(WideToASCII(beacon_name), true /* read_only */));
    EXPECT_TRUE(beacon.Map(0));
    EXPECT_TRUE(beacon.memory());

    char buffer[kSharedMemoryBytes] = {0};
    memcpy(buffer, beacon.memory(), kSharedMemoryBytes - 1);
    scoped_ptr<Version> beacon_version(Version::GetVersionFromString(buffer));
    ASSERT_TRUE(beacon_version != NULL);

    EXPECT_EQ(0,
              beacon_version->CompareTo(*test_data[i].expected_beacon_version));
  }
}
