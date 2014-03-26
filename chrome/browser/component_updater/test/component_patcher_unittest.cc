// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/component_updater/component_patcher.h"
#include "chrome/browser/component_updater/component_patcher_operation.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/component_updater/test/component_patcher_mock.h"
#include "chrome/browser/component_updater/test/component_patcher_unittest.h"
#include "chrome/browser/component_updater/test/test_installer.h"
#include "chrome/common/chrome_paths.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

base::FilePath test_file(const char* file) {
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.AppendASCII("components").AppendASCII(file);
}

}  // namespace

ComponentPatcherOperationTest::ComponentPatcherOperationTest() {
  EXPECT_TRUE(unpack_dir_.CreateUniqueTempDir());
  EXPECT_TRUE(input_dir_.CreateUniqueTempDir());
  EXPECT_TRUE(installed_dir_.CreateUniqueTempDir());
  patcher_.reset(new MockComponentPatcher());
  installer_.reset(new ReadOnlyTestInstaller(installed_dir_.path()));
}

ComponentPatcherOperationTest::~ComponentPatcherOperationTest() {
}

ComponentUnpacker::Error MockComponentPatcher::Patch(
    PatchType patch_type,
    const base::FilePath& input_file,
    const base::FilePath& patch_file,
    const base::FilePath& output_file,
    int* error) {
  *error = 0;
  int exit_code;
  if (patch_type == kPatchTypeCourgette) {
    exit_code = courgette::ApplyEnsemblePatch(input_file.value().c_str(),
                                              patch_file.value().c_str(),
                                              output_file.value().c_str());
    if (exit_code == courgette::C_OK)
      return ComponentUnpacker::kNone;
    *error = exit_code + kCourgetteErrorOffset;
  } else if (patch_type == kPatchTypeBsdiff) {
    exit_code = courgette::ApplyBinaryPatch(input_file,
                                            patch_file,
                                            output_file);
    if (exit_code == courgette::OK)
      return ComponentUnpacker::kNone;
    *error = exit_code + kBsdiffErrorOffset;
  }
  return ComponentUnpacker::kDeltaOperationFailure;
}

// Verify that a 'create' delta update operation works correctly.
TEST_F(ComponentPatcherOperationTest, CheckCreateOperation) {
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_output.bin"),
      input_dir_.path().Append(FILE_PATH_LITERAL("binary_output.bin"))));

  scoped_ptr<base::DictionaryValue> command_args(new base::DictionaryValue());
  command_args->SetString("output", "output.bin");
  command_args->SetString("sha256", binary_output_hash);
  command_args->SetString("op", "create");
  command_args->SetString("patch", "binary_output.bin");

  int error = 0;
  scoped_ptr<DeltaUpdateOp> op(new DeltaUpdateOpCreate());
  ComponentUnpacker::Error result = op->Run(command_args.get(),
                                            input_dir_.path(),
                                            unpack_dir_.path(),
                                            patcher_.get(),
                                            NULL,
                                            &error);

  EXPECT_EQ(ComponentUnpacker::kNone, result);
  EXPECT_EQ(0, error);
  EXPECT_TRUE(base::ContentsEqual(
      unpack_dir_.path().Append(FILE_PATH_LITERAL("output.bin")),
      test_file("binary_output.bin")));
}

// Verify that a 'copy' delta update operation works correctly.
TEST_F(ComponentPatcherOperationTest, CheckCopyOperation) {
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_output.bin"),
      installed_dir_.path().Append(FILE_PATH_LITERAL("binary_output.bin"))));

  scoped_ptr<base::DictionaryValue> command_args(new base::DictionaryValue());
  command_args->SetString("output", "output.bin");
  command_args->SetString("sha256", binary_output_hash);
  command_args->SetString("op", "copy");
  command_args->SetString("input", "binary_output.bin");

  int error = 0;
  scoped_ptr<DeltaUpdateOp> op(new DeltaUpdateOpCopy());
  ComponentUnpacker::Error result = op->Run(command_args.get(),
                                            input_dir_.path(),
                                            unpack_dir_.path(),
                                            patcher_.get(),
                                            installer_.get(),
                                            &error);
  EXPECT_EQ(ComponentUnpacker::kNone, result);
  EXPECT_EQ(0, error);
  EXPECT_TRUE(base::ContentsEqual(
      unpack_dir_.path().Append(FILE_PATH_LITERAL("output.bin")),
      test_file("binary_output.bin")));
}

// Verify that a 'courgette' delta update operation works correctly.
TEST_F(ComponentPatcherOperationTest, CheckCourgetteOperation) {
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_input.bin"),
      installed_dir_.path().Append(FILE_PATH_LITERAL("binary_input.bin"))));
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_courgette_patch.bin"),
      input_dir_.path().Append(
          FILE_PATH_LITERAL("binary_courgette_patch.bin"))));

  scoped_ptr<base::DictionaryValue> command_args(new base::DictionaryValue());
  command_args->SetString("output", "output.bin");
  command_args->SetString("sha256", binary_output_hash);
  command_args->SetString("op", "courgette");
  command_args->SetString("input", "binary_input.bin");
  command_args->SetString("patch", "binary_courgette_patch.bin");

  int error = 0;
  scoped_ptr<DeltaUpdateOp> op(new DeltaUpdateOpPatchCourgette());
  ComponentUnpacker::Error result = op->Run(command_args.get(),
                                            input_dir_.path(),
                                            unpack_dir_.path(),
                                            patcher_.get(),
                                            installer_.get(),
                                            &error);
  EXPECT_EQ(ComponentUnpacker::kNone, result);
  EXPECT_EQ(0, error);
  EXPECT_TRUE(base::ContentsEqual(
      unpack_dir_.path().Append(FILE_PATH_LITERAL("output.bin")),
      test_file("binary_output.bin")));
}

// Verify that a 'bsdiff' delta update operation works correctly.
TEST_F(ComponentPatcherOperationTest, CheckBsdiffOperation) {
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_input.bin"),
      installed_dir_.path().Append(FILE_PATH_LITERAL("binary_input.bin"))));
  EXPECT_TRUE(base::CopyFile(
      test_file("binary_bsdiff_patch.bin"),
      input_dir_.path().Append(FILE_PATH_LITERAL("binary_bsdiff_patch.bin"))));

  scoped_ptr<base::DictionaryValue> command_args(new base::DictionaryValue());
  command_args->SetString("output", "output.bin");
  command_args->SetString("sha256", binary_output_hash);
  command_args->SetString("op", "courgette");
  command_args->SetString("input", "binary_input.bin");
  command_args->SetString("patch", "binary_bsdiff_patch.bin");

  int error = 0;
  scoped_ptr<DeltaUpdateOp> op(new DeltaUpdateOpPatchBsdiff());
  ComponentUnpacker::Error result = op->Run(command_args.get(),
                                            input_dir_.path(),
                                            unpack_dir_.path(),
                                            patcher_.get(),
                                            installer_.get(),
                                            &error);
  EXPECT_EQ(ComponentUnpacker::kNone, result);
  EXPECT_EQ(0, error);
  EXPECT_TRUE(base::ContentsEqual(
      unpack_dir_.path().Append(FILE_PATH_LITERAL("output.bin")),
      test_file("binary_output.bin")));
}

}  // namespace component_updater
