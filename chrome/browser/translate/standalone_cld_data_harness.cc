// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "standalone_cld_data_harness.h"

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This has to match what's in chrome_translate_client.cc
const base::FilePath::CharType kStandaloneDataFileName[] =
    FILE_PATH_LITERAL("cld2_data.bin");

}  // namespace

namespace test {

StandaloneCldDataHarness::StandaloneCldDataHarness() {
  // Constructor does nothing in all cases. See Init() for initialization.
}

StandaloneCldDataHarness::~StandaloneCldDataHarness() {
  VLOG(1) << "Tearing down CLD data harness";
  DeleteStandaloneDataFile();
}

void StandaloneCldDataHarness::Init() {
  VLOG(1) << "Initializing CLD data harness";
  // Dynamic data mode is enabled and we are using a standalone file.
  ASSERT_NO_FATAL_FAILURE(CopyStandaloneDataFile());
}

void StandaloneCldDataHarness::GetStandaloneDataFileSource(
    base::FilePath* out_path) {
  CldDataHarness::GetTestDataSourceDirectory(out_path);
  *out_path = out_path->Append(FILE_PATH_LITERAL("_platform_specific"))
                  .Append(FILE_PATH_LITERAL("all"))
                  .Append(kStandaloneDataFileName);
}

// Using the USER_DATA_DIR not only mimics true functionality, but also is
// important to test isolation. Each test gets its own USER_DATA_DIR, which
// ensures proper isolation between test processes running in parallel.
void StandaloneCldDataHarness::GetStandaloneDataFileDestination(
    base::FilePath* out_path) {
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, out_path));
  *out_path = out_path->Append(kStandaloneDataFileName);
}

void StandaloneCldDataHarness::DeleteStandaloneDataFile() {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(GetStandaloneDataFileDestination(&path));
  VLOG(1) << "Deleting CLD test data file from " << path.value();
  base::DeleteFile(path, false);
}

void StandaloneCldDataHarness::CopyStandaloneDataFile() {
  DeleteStandaloneDataFile();  // sanity: blow away any old copies.
  base::FilePath target_file;
  GetStandaloneDataFileDestination(&target_file);
  base::FilePath target_dir = target_file.DirName();
  ASSERT_TRUE(base::CreateDirectoryAndGetError(target_dir, NULL));
  base::FilePath source_file;
  GetStandaloneDataFileSource(&source_file);
  VLOG(1) << "Copying CLD test data file from " << source_file.value() << " to "
          << target_file.value();
  ASSERT_TRUE(base::CopyFile(source_file, target_file));
  ASSERT_TRUE(base::PathExists(target_file));
}

scoped_ptr<CldDataHarness> CreateCldDataHarness() {
  scoped_ptr<CldDataHarness> result(new StandaloneCldDataHarness());
  return result.Pass();
}

}  // namespace test
