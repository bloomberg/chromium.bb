// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini_integration.h"

#include "base/logging.h"
#include "chrome/installer/zucchini/buffer_view.h"
#include "chrome/installer/zucchini/mapped_file.h"
#include "chrome/installer/zucchini/patch_reader.h"

namespace zucchini {

status::Code Apply(const base::FilePath& old_path,
                   const base::FilePath& patch_path,
                   const base::FilePath& new_path) {
  MappedFileReader patch_file(patch_path);
  if (!patch_file.IsValid())
    return status::kStatusFileReadError;

  auto patch_reader =
      zucchini::EnsemblePatchReader::Create(patch_file.region());
  if (!patch_reader.has_value()) {
    LOG(ERROR) << "Error reading patch header.";
    return status::kStatusPatchReadError;
  }

  MappedFileReader old_file(old_path);
  if (!old_file.IsValid())
    return status::kStatusFileReadError;
  if (!patch_reader->CheckOldFile(old_file.region())) {
    LOG(ERROR) << "Invalid old_file.";
    return status::kStatusInvalidOldImage;
  }

  zucchini::PatchHeader header = patch_reader->header();
  // By default, delete output on destruction, to avoid having lingering files
  // in case of a failure. On Windows deletion can be done by the OS.
  MappedFileWriter new_file(new_path, header.new_size);
  if (!new_file.IsValid())
    return status::kStatusFileWriteError;

  zucchini::status::Code result =
      zucchini::Apply(old_file.region(), *patch_reader, new_file.region());
  if (result != status::kStatusSuccess) {
    LOG(ERROR) << "Fatal error encountered while applying patch.";
    return result;
  }

  // Successfully patch |new_file|. Explicitly request file to be kept.
  if (!new_file.Keep())
    return status::kStatusFileWriteError;
  return status::kStatusSuccess;
}

}  // namespace zucchini
