// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_browser_test_utils.h"

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/component_updater/cld_component_installer.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This constant yields the version of the CRX that has been extracted into
// the test data directory, and must be kept in sync with what is there.
// A reciprocal comment has been placed in
// chrome/test/data/cld2_component/README.chromium; don't update one without
// updating the other.
#if defined(CLD2_DYNAMIC_MODE)
const base::FilePath::CharType kCrxVersion[] = FILE_PATH_LITERAL("160");

void GetTestDataSourceDirectory(base::FilePath* out_path) {
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, out_path));
  *out_path = out_path->Append(FILE_PATH_LITERAL("cld2_component"))
      .Append(kCrxVersion);
}
#endif  // defined(CLD2_DYNAMIC_MODE)

#if defined(CLD2_DYNAMIC_MODE) && !defined(CLD2_IS_COMPONENT)
void GetStandaloneDataFileSource(base::FilePath* out_path) {
  GetTestDataSourceDirectory(out_path);
  *out_path = out_path->Append(FILE_PATH_LITERAL("_platform_specific"))
      .Append(FILE_PATH_LITERAL("all"))
      .Append(chrome::kCLDDataFilename);
}

// Using the USER_DATA_DIR not only mimics true functionality, but also is
// important to test isolation. Each test gets its own USER_DATA_DIR, which
// ensures proper isolation between test processes running in parallel.
void GetStandaloneDataFileDestination(base::FilePath* out_path) {
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, out_path));
  *out_path = out_path->Append(chrome::kCLDDataFilename);
}

void DeleteStandaloneDataFile() {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(GetStandaloneDataFileDestination(&path));
  DLOG(INFO) << "Deleting CLD test data file from " << path.value();
  base::DeleteFile(path, false);
}

void CopyStandaloneDataFile() {
  DeleteStandaloneDataFile();  // sanity: blow away any old copies.
  base::FilePath target_file;
  GetStandaloneDataFileDestination(&target_file);
  base::FilePath target_dir = target_file.DirName();
  ASSERT_TRUE(base::CreateDirectoryAndGetError(target_dir, NULL));
  base::FilePath source_file;
  GetStandaloneDataFileSource(&source_file);
  DLOG(INFO) << "Copying CLD test data file from " << source_file.value()
             << " to " << target_file.value();
  ASSERT_TRUE(base::CopyFile(source_file, target_file));
  ASSERT_TRUE(base::PathExists(target_file));
}
#endif  // defined(CLD2_DYNAMIC_MODE) && !defined(CLD2_IS_COMPONENT)

#if defined(CLD2_DYNAMIC_MODE) && defined(CLD2_IS_COMPONENT)
// DIR_COMPONENT_CLD2 is also defined as being relative to USER_DATA_DIR, so
// like GetStandaloneDataFileDestination, this is safe to run in multiple
// parallel test processes.
void GetExtractedComponentDestination(base::FilePath* out_path) {
  ASSERT_TRUE(PathService::Get(chrome::DIR_COMPONENT_CLD2, out_path));
}

void GetComponentDataFileDestination(base::FilePath* out_path) {
  GetExtractedComponentDestination(out_path);
  *out_path = out_path->Append(kCrxVersion)
      .Append(FILE_PATH_LITERAL("_platform_specific"))
      .Append(FILE_PATH_LITERAL("all"))
      .Append(chrome::kCLDDataFilename);
}

void DeleteComponentTree() {
  base::FilePath tree_path;
  ASSERT_NO_FATAL_FAILURE(GetExtractedComponentDestination(&tree_path));
  DLOG(INFO) << "Deleting CLD component test files from " << tree_path.value();
  base::DeleteFile(tree_path, true);
}

void CopyComponentTree() {
  DeleteComponentTree();  // sanity: blow away any old copies.
  base::FilePath target_dir;
  GetExtractedComponentDestination(&target_dir);
  base::FilePath source_dir;
  GetTestDataSourceDirectory(&source_dir);
  DLOG(INFO) << "Copying CLD component test files from " << source_dir.value()
             << " to " << target_dir.value();
  ASSERT_TRUE(base::CreateDirectoryAndGetError(target_dir, NULL));
  ASSERT_TRUE(base::CopyDirectory(source_dir, target_dir, true));
  ASSERT_TRUE(base::PathExists(target_dir));
  base::FilePath check_path;
  GetComponentDataFileDestination(&check_path);
  ASSERT_TRUE(base::PathExists(check_path));
}
#endif  // defined(CLD2_DYNAMIC_MODE) && defined(CLD2_IS_COMPONENT)

}  // namespace

namespace test {

ScopedCLDDynamicDataHarness::ScopedCLDDynamicDataHarness() {
  // Constructor does nothing in all cases. See Init() for initialization.
}

ScopedCLDDynamicDataHarness::~ScopedCLDDynamicDataHarness() {
  DLOG(INFO) << "Tearing down CLD data harness";
#if defined(CLD2_DYNAMIC_MODE) && defined(CLD2_IS_COMPONENT)
  // Dynamic data mode is enabled and we are using the component updater.
  component_updater::CldComponentInstallerTraits::SetLatestCldDataFile(
      base::FilePath());
  DeleteComponentTree();
#elif defined(CLD2_DYNAMIC_MODE)
  // Dynamic data mode is enabled and we are using a standalone file.
  ClearStandaloneDataFileState();
  DeleteStandaloneDataFile();
#endif  // defined(CLD2_DYNAMIC_MODE)
}

void ScopedCLDDynamicDataHarness::Init() {
  DLOG(INFO) << "Initializing CLD data harness";
#if defined(CLD2_DYNAMIC_MODE) && defined(CLD2_IS_COMPONENT)
  // Dynamic data mode is enabled and we are using the component updater.
  ASSERT_NO_FATAL_FAILURE(CopyComponentTree());
  base::FilePath data_file;
  GetComponentDataFileDestination(&data_file);
  component_updater::CldComponentInstallerTraits::SetLatestCldDataFile(
      data_file);
  base::FilePath result = component_updater::GetLatestCldDataFile();
  ASSERT_EQ(data_file, result);
#elif defined(CLD2_DYNAMIC_MODE)
  // Dynamic data mode is enabled and we are using a standalone file.
  ASSERT_NO_FATAL_FAILURE(ClearStandaloneDataFileState());
  ASSERT_NO_FATAL_FAILURE(CopyStandaloneDataFile());
#endif  // defined(CLD2_DYNAMIC_MODE)
}

void ScopedCLDDynamicDataHarness::ClearStandaloneDataFileState() {
#if defined(CLD2_DYNAMIC_MODE)
  DLOG(INFO) << "Clearing CLD data file state";
  // This code must live within the class in order to gain "friend" access.
  base::AutoLock lock(TranslateTabHelper::s_file_lock_.Get());
  if (TranslateTabHelper::s_cached_file_) {
    // Leaks any open handle, no way to avoid safely.
    TranslateTabHelper::s_cached_file_ = NULL;
    TranslateTabHelper::s_cached_data_offset_ = 0;
    TranslateTabHelper::s_cached_data_length_ = 0;
  }
#endif  // defined(CLD2_DYNAMIC_MODE)
}

}  // namespace test
