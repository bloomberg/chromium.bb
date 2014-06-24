// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "component_cld_data_harness.h"

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/component_updater/cld_component_installer.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This has to match what's in cld_component_installer.cc.
const base::FilePath::CharType kComponentDataFileName[] =
    FILE_PATH_LITERAL("cld2_data.bin");

}  // namespace

namespace test {

ComponentCldDataHarness::ComponentCldDataHarness() {
  // Constructor does nothing in all cases. See Init() for initialization.
}

ComponentCldDataHarness::~ComponentCldDataHarness() {
  VLOG(1) << "Tearing down CLD data harness";
  // Dynamic data mode is enabled and we are using the component updater.
  component_updater::CldComponentInstallerTraits::SetLatestCldDataFile(
      base::FilePath());
  ClearComponentDataFileState();
  DeleteComponentTree();
}

void ComponentCldDataHarness::Init() {
  VLOG(1) << "Initializing CLD data harness";
  // Dynamic data mode is enabled and we are using the component updater.
  ASSERT_NO_FATAL_FAILURE(CopyComponentTree());
  base::FilePath data_file;
  GetComponentDataFileDestination(&data_file);
  component_updater::CldComponentInstallerTraits::SetLatestCldDataFile(
      data_file);
}

void ComponentCldDataHarness::ClearComponentDataFileState() {
  VLOG(1) << "Clearing component CLD data file state";
  base::FilePath nothing;
  component_updater::CldComponentInstallerTraits::SetLatestCldDataFile(nothing);
}

// DIR_COMPONENT_CLD2 is also defined as being relative to USER_DATA_DIR, so
// like GetStandaloneDataFileDestination, this is safe to run in multiple
// parallel test processes.
void ComponentCldDataHarness::GetExtractedComponentDestination(
    base::FilePath* out_path) {
  ASSERT_TRUE(PathService::Get(chrome::DIR_COMPONENT_CLD2, out_path));
}

void ComponentCldDataHarness::GetComponentDataFileDestination(
    base::FilePath* out_path) {
  GetExtractedComponentDestination(out_path);
  *out_path = out_path->Append(CldDataHarness::GetTestDataSourceCrxVersion())
                  .Append(FILE_PATH_LITERAL("_platform_specific"))
                  .Append(FILE_PATH_LITERAL("all"))
                  .Append(kComponentDataFileName);
}

void ComponentCldDataHarness::DeleteComponentTree() {
  base::FilePath tree_path;
  ASSERT_NO_FATAL_FAILURE(GetExtractedComponentDestination(&tree_path));
  VLOG(1) << "Deleting CLD component test files from " << tree_path.value();
  base::DeleteFile(tree_path, true);
}

void ComponentCldDataHarness::CopyComponentTree() {
  DeleteComponentTree();  // sanity: blow away any old copies.
  base::FilePath target_dir;
  GetExtractedComponentDestination(&target_dir);
  base::FilePath source_dir;
  CldDataHarness::GetTestDataSourceDirectory(&source_dir);
  VLOG(1) << "Copying CLD component test files from " << source_dir.value()
          << " to " << target_dir.value();
  ASSERT_TRUE(base::CreateDirectoryAndGetError(target_dir, NULL));
  ASSERT_TRUE(base::CopyDirectory(source_dir, target_dir, true));
  ASSERT_TRUE(base::PathExists(target_dir));
  base::FilePath check_path;
  GetComponentDataFileDestination(&check_path);
  ASSERT_TRUE(base::PathExists(check_path));
}

scoped_ptr<CldDataHarness> CreateCldDataHarness() {
  scoped_ptr<CldDataHarness> result(new ComponentCldDataHarness());
  return result.Pass();
}

}  // namespace test
