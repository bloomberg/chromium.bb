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

// Verify that a 'courgette' delta update operation works correctly.
TEST_F(ComponentPatcherOperationTest, CheckCourgetteOperation) {
  EXPECT_TRUE(file_util::CopyFile(
      test_file("binary_input.bin"),
      installed_dir_.path().Append(FILE_PATH_LITERAL("binary_input.bin"))));
  EXPECT_TRUE(file_util::CopyFile(
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
  EXPECT_TRUE(file_util::ContentsEqual(
      unpack_dir_.path().Append(FILE_PATH_LITERAL("output.bin")),
      test_file("binary_output.bin")));
}

// Verify that a 'bsdiff' delta update operation works correctly.
TEST_F(ComponentPatcherOperationTest, CheckBsdiffOperation) {
  EXPECT_TRUE(file_util::CopyFile(
      test_file("binary_input.bin"),
      installed_dir_.path().Append(FILE_PATH_LITERAL("binary_input.bin"))));
  EXPECT_TRUE(file_util::CopyFile(
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
  EXPECT_TRUE(file_util::ContentsEqual(
      unpack_dir_.path().Append(FILE_PATH_LITERAL("output.bin")),
      test_file("binary_output.bin")));
}
