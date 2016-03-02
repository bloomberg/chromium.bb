// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_old_versions.h"

#include <set>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/installer/test/alternate_version_generator.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

class MockInstallerState : public InstallerState {
 public:
  explicit MockInstallerState(const base::FilePath& target_path)
      : InstallerState(InstallerState::USER_LEVEL) {
    target_path_ = target_path;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstallerState);
};

const base::char16 kVersionA[] = L"47.0.0.0";
const base::char16 kVersionB[] = L"48.0.0.0";

class DeleteOldVersionsTest : public testing::Test {
 protected:
  DeleteOldVersionsTest() = default;

  void DeleteOldVersions() {
    MockInstallerState installer_state(install_dir());
    scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
    AddDeleteOldVersionsWorkItem(installer_state, work_item_list.get());
    work_item_list->Do();
  }

  bool CreateInstallDir() { return install_dir_.CreateUniqueTempDir(); }

  // Creates an executable with |name| and |version| in |install_dir_|.
  bool CreateExecutable(const base::string16& name,
                        const base::string16& version) {
    base::FilePath current_exe_path;
    return base::PathService::Get(base::FILE_EXE, &current_exe_path) &&
           upgrade_test::GenerateSpecificPEFileVersion(
               current_exe_path, install_dir().Append(name),
               base::Version(base::UTF16ToUTF8(version)));
  }

  // Creates a version directory named |name| in |install_dir_|.
  bool CreateVersionDirectory(const base::string16& name) {
    static const char kDummyContent[] = "dummy";
    const base::FilePath version_dir_path(install_dir().Append(name));

    return base::CreateDirectory(install_dir().Append(name)) &&
           base::CreateDirectory(version_dir_path.Append(L"Installer")) &&
           base::WriteFile(version_dir_path.Append(L"chrome.dll"),
                           kDummyContent, sizeof(kDummyContent)) &&
           base::WriteFile(version_dir_path.Append(L"nacl64.exe"),
                           kDummyContent, sizeof(kDummyContent)) &&
           base::WriteFile(version_dir_path.Append(L"icudtl.dat"),
                           kDummyContent, sizeof(kDummyContent)) &&
           base::WriteFile(version_dir_path.Append(L"Installer\\setup.exe"),
                           kDummyContent, sizeof(kDummyContent));
  }

  // Returns the relative paths of all files and directories in |install_dir_|.
  using FilePathSet = std::set<base::FilePath>;
  FilePathSet GetInstallDirContent() const {
    std::set<base::FilePath> content;
    base::FileEnumerator file_enumerator(
        install_dir(), true,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    for (base::FilePath path = file_enumerator.Next(); !path.empty();
         path = file_enumerator.Next()) {
      DCHECK(base::StartsWith(path.value(), install_dir().value(),
                              base::CompareCase::SENSITIVE));
      content.insert(base::FilePath(
          path.value().substr(install_dir().value().size() + 1)));
    }
    return content;
  }

  // Adds to |file_path_set| all files and directories that are expected to be
  // found in the version directory |version| before any attempt to delete it.
  void AddVersionFiles(const base::string16& version,
                       FilePathSet* file_path_set) {
    file_path_set->insert(base::FilePath(version));
    file_path_set->insert(base::FilePath(version).Append(L"chrome.dll"));
    file_path_set->insert(base::FilePath(version).Append(L"nacl64.exe"));
    file_path_set->insert(base::FilePath(version).Append(L"icudtl.dat"));
    file_path_set->insert(base::FilePath(version).Append(L"Installer"));
    file_path_set->insert(
        base::FilePath(version).Append(L"Installer\\setup.exe"));
  }

  base::FilePath install_dir() const { return install_dir_.path(); }

 private:
  base::ScopedTempDir install_dir_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOldVersionsTest);
};

}  // namespace

// An old executable without a matching directory should be deleted.
TEST_F(DeleteOldVersionsTest, DeleteOldExecutableWithoutMatchingDirectory) {
  ASSERT_TRUE(CreateInstallDir());
  ASSERT_TRUE(CreateExecutable(installer::kChromeOldExe, kVersionA));

  DeleteOldVersions();
  EXPECT_TRUE(GetInstallDirContent().empty());
}

// chrome.exe and new_chrome.exe should never be deleted.
TEST_F(DeleteOldVersionsTest, DeleteNewExecutablesWithoutMatchingDirectory) {
  ASSERT_TRUE(CreateInstallDir());
  ASSERT_TRUE(CreateExecutable(installer::kChromeExe, kVersionA));
  ASSERT_TRUE(CreateExecutable(installer::kChromeNewExe, kVersionB));

  DeleteOldVersions();
  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeExe));
  expected_install_dir_content.insert(base::FilePath(installer::kChromeNewExe));
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// A directory without a matching executable should be deleted.
TEST_F(DeleteOldVersionsTest, DeleteDirectoryWithoutMatchingExecutable) {
  ASSERT_TRUE(CreateInstallDir());
  ASSERT_TRUE(CreateVersionDirectory(kVersionA));

  DeleteOldVersions();
  EXPECT_TRUE(GetInstallDirContent().empty());
}

// A pair of matching old executable/version directory that is not in use should
// be deleted.
TEST_F(DeleteOldVersionsTest, DeleteOldExecutableWithMatchingDirectory) {
  ASSERT_TRUE(CreateInstallDir());
  ASSERT_TRUE(CreateExecutable(installer::kChromeOldExe, kVersionA));
  ASSERT_TRUE(CreateVersionDirectory(kVersionA));

  DeleteOldVersions();
  EXPECT_TRUE(GetInstallDirContent().empty());
}

// chrome.exe, new_chrome.exe and their matching version directories should
// never be deleted.
TEST_F(DeleteOldVersionsTest, DeleteNewExecutablesWithMatchingDirectory) {
  ASSERT_TRUE(CreateInstallDir());
  ASSERT_TRUE(CreateExecutable(installer::kChromeExe, kVersionA));
  ASSERT_TRUE(CreateVersionDirectory(kVersionA));
  ASSERT_TRUE(CreateExecutable(installer::kChromeNewExe, kVersionB));
  ASSERT_TRUE(CreateVersionDirectory(kVersionB));

  DeleteOldVersions();

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeExe));
  AddVersionFiles(kVersionA, &expected_install_dir_content);
  expected_install_dir_content.insert(base::FilePath(installer::kChromeNewExe));
  AddVersionFiles(kVersionB, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// chrome.exe, new_chrome.exe and their matching version directories should
// never be deleted, even when files named old_chrome*.exe have the same
// versions as chrome.exe/new_chrome.exe. The old_chrome*.exe files, however,
// should be deleted.
TEST_F(DeleteOldVersionsTest,
       DeleteNewExecutablesWithMatchingDirectoryAndOldExecutables) {
  ASSERT_TRUE(CreateInstallDir());
  ASSERT_TRUE(CreateExecutable(installer::kChromeExe, kVersionA));
  ASSERT_TRUE(CreateVersionDirectory(kVersionA));
  ASSERT_TRUE(CreateExecutable(installer::kChromeNewExe, kVersionB));
  ASSERT_TRUE(CreateVersionDirectory(kVersionB));
  ASSERT_TRUE(CreateExecutable(L"old_chrome.exe", kVersionA));
  ASSERT_TRUE(CreateExecutable(L"old_chrome2.exe", kVersionB));

  DeleteOldVersions();

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeExe));
  AddVersionFiles(kVersionA, &expected_install_dir_content);
  expected_install_dir_content.insert(base::FilePath(installer::kChromeNewExe));
  AddVersionFiles(kVersionB, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// No file should be deleted for a given version if the executable is in use.
TEST_F(DeleteOldVersionsTest, DeleteVersionWithExecutableInUse) {
  ASSERT_TRUE(CreateInstallDir());
  ASSERT_TRUE(CreateExecutable(installer::kChromeOldExe, kVersionA));
  ASSERT_TRUE(CreateVersionDirectory(kVersionA));

  base::File file_in_use(install_dir().Append(installer::kChromeOldExe),
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file_in_use.IsValid());

  DeleteOldVersions();

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeOldExe));
  AddVersionFiles(kVersionA, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// No file should be deleted for a given version if a .dll file in the version
// directory is in use.
TEST_F(DeleteOldVersionsTest, DeleteVersionWithVersionDirectoryDllInUse) {
  ASSERT_TRUE(CreateInstallDir());
  ASSERT_TRUE(CreateExecutable(installer::kChromeOldExe, kVersionA));
  ASSERT_TRUE(CreateVersionDirectory(kVersionA));

  base::File file_in_use(install_dir().Append(kVersionA).Append(L"chrome.dll"),
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file_in_use.IsValid());

  DeleteOldVersions();

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeOldExe));
  AddVersionFiles(kVersionA, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// No file should be deleted for a given version if a .exe file in the version
// directory is in use.
TEST_F(DeleteOldVersionsTest, DeleteVersionWithVersionDirectoryExeInUse) {
  ASSERT_TRUE(CreateInstallDir());
  ASSERT_TRUE(CreateExecutable(installer::kChromeOldExe, kVersionA));
  ASSERT_TRUE(CreateVersionDirectory(kVersionA));

  base::File file_in_use(
      install_dir().Append(kVersionA).Append(L"Installer\\setup.exe"),
      base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file_in_use.IsValid());

  DeleteOldVersions();

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeOldExe));
  AddVersionFiles(kVersionA, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// If an installation directory contains a file named chrome.exe with a matching
// directory v1 and a file name old_chrome.exe with a matching directory v2,
// old_chrome.exe and v2 should be deleted but chrome.exe and v1 shouldn't.
TEST_F(DeleteOldVersionsTest, TypicalAfterRenameState) {
  ASSERT_TRUE(CreateInstallDir());
  ASSERT_TRUE(CreateExecutable(installer::kChromeOldExe, kVersionA));
  ASSERT_TRUE(CreateVersionDirectory(kVersionA));
  ASSERT_TRUE(CreateExecutable(installer::kChromeExe, kVersionB));
  ASSERT_TRUE(CreateVersionDirectory(kVersionB));

  DeleteOldVersions();

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeExe));
  AddVersionFiles(kVersionB, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

}  // namespace installer
