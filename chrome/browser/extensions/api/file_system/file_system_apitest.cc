// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_files_service.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

namespace {

class AppInstallObserver : public content::NotificationObserver {
 public:
  AppInstallObserver(
      base::Callback<void(const Extension*)> callback)
      : callback_(callback) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_EXTENSION_LOADED,
                   content::NotificationService::AllSources());
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    EXPECT_EQ(chrome::NOTIFICATION_EXTENSION_LOADED, type);
    callback_.Run(content::Details<const Extension>(details).ptr());
  }

 private:
  content::NotificationRegistrar registrar_;
  base::Callback<void(const Extension*)> callback_;
  DISALLOW_COPY_AND_ASSIGN(AppInstallObserver);
};

void SetLastChooseEntryDirectory(const base::FilePath& choose_entry_directory,
                                 ExtensionPrefs* prefs,
                                 const Extension* extension) {
  file_system_api::SetLastChooseEntryDirectory(
      prefs, extension->id(), choose_entry_directory);
}

void SetLastChooseEntryDirectoryToAppDirectory(
    ExtensionPrefs* prefs,
    const Extension* extension) {
  file_system_api::SetLastChooseEntryDirectory(
      prefs, extension->id(), extension->path());
}

void AddSavedEntry(const base::FilePath& path_to_save,
                   bool is_directory,
                   apps::SavedFilesService* service,
                   const Extension* extension) {
  service->RegisterFileEntry(
      extension->id(), "magic id", path_to_save, is_directory);
}

#if defined(OS_WIN) || defined(OS_POSIX)
#if defined(OS_WIN)
  const int kGraylistedPath = base::DIR_PROFILE;
#elif defined(OS_POSIX)
  const int kGraylistedPath = base::DIR_HOME;
#endif
#endif

}  // namespace

class FileSystemApiTest : public PlatformAppBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    test_root_folder_ = test_data_dir_.AppendASCII("api_test")
        .AppendASCII("file_system");
    FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
        "test_root", test_root_folder_);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ClearCommandLineArgs();
  }

  virtual void TearDown() OVERRIDE {
    FileSystemChooseEntryFunction::StopSkippingPickerForTest();
    PlatformAppBrowserTest::TearDown();
  };

 protected:
  base::FilePath TempFilePath(const std::string& destination_name,
                              bool copy_gold) {
    if (!temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "CreateUniqueTempDir failed";
      return base::FilePath();
    }
    FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
        "test_temp", temp_dir_.path());

    base::FilePath destination = temp_dir_.path().AppendASCII(destination_name);
    if (copy_gold) {
      base::FilePath source = test_root_folder_.AppendASCII("gold.txt");
      EXPECT_TRUE(base::CopyFile(source, destination));
    }
    return destination;
  }

  std::vector<base::FilePath> TempFilePaths(
      const std::vector<std::string>& destination_names,
      bool copy_gold) {
    if (!temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "CreateUniqueTempDir failed";
      return std::vector<base::FilePath>();
    }
    FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
        "test_temp", temp_dir_.path());

    std::vector<base::FilePath> result;
    for (std::vector<std::string>::const_iterator it =
             destination_names.begin();
         it != destination_names.end(); ++it) {
      base::FilePath destination = temp_dir_.path().AppendASCII(*it);
      if (copy_gold) {
        base::FilePath source = test_root_folder_.AppendASCII("gold.txt");
        EXPECT_TRUE(base::CopyFile(source, destination));
      }
      result.push_back(destination);
    }
    return result;
  }

  void CheckStoredDirectoryMatches(const base::FilePath& filename) {
    const Extension* extension = GetSingleLoadedExtension();
    ASSERT_TRUE(extension);
    std::string extension_id = extension->id();
    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
    base::FilePath stored_value;
    if (filename.empty()) {
      EXPECT_FALSE(file_system_api::GetLastChooseEntryDirectory(
          prefs, extension_id, &stored_value));
    } else {
      EXPECT_TRUE(file_system_api::GetLastChooseEntryDirectory(
          prefs, extension_id, &stored_value));
      EXPECT_EQ(base::MakeAbsoluteFilePath(filename.DirName()),
                base::MakeAbsoluteFilePath(stored_value));
    }
  }

  base::FilePath test_root_folder_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiGetDisplayPath) {
  base::FilePath test_file = test_root_folder_.AppendASCII("gold.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/get_display_path"))
      << message_;
}

#if defined(OS_WIN) || defined(OS_POSIX)
IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiGetDisplayPathPrettify) {
#if defined(OS_WIN)
  int override = base::DIR_PROFILE;
#elif defined(OS_POSIX)
  int override = base::DIR_HOME;
#endif
  ASSERT_TRUE(PathService::OverrideAndCreateIfNeeded(override,
      test_root_folder_, false));

  base::FilePath test_file = test_root_folder_.AppendASCII("gold.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_display_path_prettify")) << message_;
}
#endif

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiGetDisplayPathPrettifyMac) {
  // On Mac, "test.localized" will be localized into just "test".
  base::FilePath test_path = TempFilePath("test.localized", false);
  ASSERT_TRUE(file_util::CreateDirectory(test_path));

  base::FilePath test_file = test_path.AppendASCII("gold.txt");
  base::FilePath source = test_root_folder_.AppendASCII("gold.txt");
  EXPECT_TRUE(base::CopyFile(source, test_file));

  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_display_path_prettify_mac")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenExistingFileTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenExistingFileUsingPreviousPathTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::
      SkipPickerAndSelectSuggestedPathForTest();
  {
    AppInstallObserver observer(
        base::Bind(SetLastChooseEntryDirectory,
                   test_file.DirName(),
                   ExtensionPrefs::Get(profile())));
    ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
        << message_;
  }
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiOpenExistingFilePreviousPathDoesNotExistTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  ASSERT_TRUE(PathService::OverrideAndCreateIfNeeded(
      chrome::DIR_USER_DOCUMENTS, test_file.DirName(), false));
  FileSystemChooseEntryFunction::
      SkipPickerAndSelectSuggestedPathForTest();
  {
    AppInstallObserver observer(base::Bind(
        SetLastChooseEntryDirectory,
        test_file.DirName().Append(
            base::FilePath::FromUTF8Unsafe("fake_directory_does_not_exist")),
        ExtensionPrefs::Get(profile())));
    ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
        << message_;
  }
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenExistingFileDefaultPathTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  ASSERT_TRUE(PathService::OverrideAndCreateIfNeeded(
      chrome::DIR_USER_DOCUMENTS, test_file.DirName(), false));
  FileSystemChooseEntryFunction::
      SkipPickerAndSelectSuggestedPathForTest();
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenMultipleSuggested) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  ASSERT_TRUE(PathService::OverrideAndCreateIfNeeded(
      chrome::DIR_USER_DOCUMENTS, test_file.DirName(), false));
  FileSystemChooseEntryFunction::SkipPickerAndSelectSuggestedPathForTest();
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_multiple_with_suggested_name"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenMultipleExistingFilesTest) {
  std::vector<std::string> names;
  names.push_back("open_existing1.txt");
  names.push_back("open_existing2.txt");
  std::vector<base::FilePath> test_files = TempFilePaths(names, true);
  ASSERT_EQ(2u, test_files.size());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathsForTest(
      &test_files);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_multiple_existing"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenDirectoryTest) {
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryWithWriteTest) {
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(
      RunPlatformAppTest("api_test/file_system/open_directory_with_write"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryWithoutPermissionTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_directory_without_permission"))
      << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryWithOnlyWritePermissionTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_directory_with_only_write"))
      << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

#if defined(OS_WIN) || defined(OS_POSIX)
IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryOnGraylistAndAllowTest) {
  FileSystemChooseEntryFunction::SkipDirectoryConfirmationForTest();
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  ASSERT_TRUE(PathService::OverrideAndCreateIfNeeded(
      kGraylistedPath, test_directory, false));
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryOnGraylistTest) {
  FileSystemChooseEntryFunction::AutoCancelDirectoryConfirmationForTest();
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  ASSERT_TRUE(PathService::OverrideAndCreateIfNeeded(
      kGraylistedPath, test_directory, false));
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory_cancel"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryContainingGraylistTest) {
  FileSystemChooseEntryFunction::AutoCancelDirectoryConfirmationForTest();
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  base::FilePath parent_directory = test_directory.DirName();
  ASSERT_TRUE(PathService::OverrideAndCreateIfNeeded(
      kGraylistedPath, test_directory, false));
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &parent_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory_cancel"))
      << message_;
  CheckStoredDirectoryMatches(test_directory);
}

// Test that choosing a subdirectory of a path does not require confirmation.
IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectorySubdirectoryOfGraylistTest) {
  // If a dialog is erroneously displayed, auto cancel it, so that the test
  // fails.
  FileSystemChooseEntryFunction::AutoCancelDirectoryConfirmationForTest();
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  base::FilePath parent_directory = test_directory.DirName();
  ASSERT_TRUE(PathService::OverrideAndCreateIfNeeded(
      kGraylistedPath, parent_directory, false));
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}
#endif  // defined(OS_WIN) || defined(OS_POSIX)

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiInvalidChooseEntryTypeTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/invalid_choose_file_type")) << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_FileSystemApiOpenExistingFileWithWriteTest DISABLED_FileSystemApiOpenExistingFileWithWriteTest
#else
#define MAYBE_FileSystemApiOpenExistingFileWithWriteTest FileSystemApiOpenExistingFileWithWriteTest
#endif
IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    MAYBE_FileSystemApiOpenExistingFileWithWriteTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_existing_with_write")) << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiOpenWritableExistingFileTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_writable_existing")) << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiOpenWritableExistingFileWithWriteTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_writable_existing_with_write")) << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenMultipleWritableExistingFilesTest) {
  std::vector<std::string> names;
  names.push_back("open_existing1.txt");
  names.push_back("open_existing2.txt");
  std::vector<base::FilePath> test_files = TempFilePaths(names, true);
  ASSERT_EQ(2u, test_files.size());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathsForTest(
      &test_files);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_multiple_writable_existing_with_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenCancelTest) {
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysCancelForTest();
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_cancel"))
      << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenBackgroundTest) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_background"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveNewFileTest) {
  base::FilePath test_file = TempFilePath("save_new.txt", false);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_new"))
      << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveExistingFileTest) {
  base::FilePath test_file = TempFilePath("save_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_existing"))
      << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiSaveNewFileWithWriteTest) {
  base::FilePath test_file = TempFilePath("save_new.txt", false);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_new_with_write"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiSaveExistingFileWithWriteTest) {
  base::FilePath test_file = TempFilePath("save_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/save_existing_with_write")) << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveMultipleFilesTest) {
  std::vector<std::string> names;
  names.push_back("save1.txt");
  names.push_back("save2.txt");
  std::vector<base::FilePath> test_files = TempFilePaths(names, false);
  ASSERT_EQ(2u, test_files.size());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathsForTest(
      &test_files);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_multiple"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveCancelTest) {
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysCancelForTest();
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_cancel"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveBackgroundTest) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_background"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiGetWritableTest) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_writable_file_entry")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiGetWritableWithWriteTest) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_writable_file_entry_with_write")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiIsWritableTest) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/is_writable_file_entry"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiIsWritableWithWritePermissionTest) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/is_writable_file_entry_with_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiRetainEntry) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/retain_entry")) << message_;
  std::vector<apps::SavedFileEntry> file_entries = apps::SavedFilesService::Get(
      profile())->GetAllFileEntries(GetSingleLoadedExtension()->id());
  ASSERT_EQ(1u, file_entries.size());
  EXPECT_EQ(test_file, file_entries[0].path);
  EXPECT_EQ(1, file_entries[0].sequence_number);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiRetainDirectoryEntry) {
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/retain_directory"))
      << message_;
  std::vector<apps::SavedFileEntry> file_entries = apps::SavedFilesService::Get(
      profile())->GetAllFileEntries(GetSingleLoadedExtension()->id());
  ASSERT_EQ(1u, file_entries.size());
  EXPECT_EQ(test_directory, file_entries[0].path);
  EXPECT_EQ(1, file_entries[0].sequence_number);
  EXPECT_TRUE(file_entries[0].is_directory);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiRestoreEntry) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  AppInstallObserver observer(
      base::Bind(AddSavedEntry,
                  test_file,
                  false,
                  apps::SavedFilesService::Get(profile())));
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/restore_entry"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiRestoreDirectoryEntry) {
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  AppInstallObserver observer(
      base::Bind(AddSavedEntry,
                 test_directory,
                 true,
                 apps::SavedFilesService::Get(profile())));
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/restore_directory"))
      << message_;
}

}  // namespace extensions
