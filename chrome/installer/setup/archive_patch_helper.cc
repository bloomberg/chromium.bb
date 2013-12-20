// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/archive_patch_helper.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/installer/util/lzma_util.h"
#include "courgette/courgette.h"
#include "third_party/bspatch/mbspatch.h"

namespace installer {

ArchivePatchHelper::ArchivePatchHelper(const base::FilePath& working_directory,
                                       const base::FilePath& compressed_archive,
                                       const base::FilePath& patch_source,
                                       const base::FilePath& target)
    : working_directory_(working_directory),
      compressed_archive_(compressed_archive),
      patch_source_(patch_source),
      target_(target) {}

ArchivePatchHelper::~ArchivePatchHelper() {}

// static
bool ArchivePatchHelper::UncompressAndPatch(
    const base::FilePath& working_directory,
    const base::FilePath& compressed_archive,
    const base::FilePath& patch_source,
    const base::FilePath& target) {
  ArchivePatchHelper instance(working_directory, compressed_archive,
                              patch_source, target);
  return (instance.Uncompress(NULL) &&
          (instance.EnsemblePatch() || instance.BinaryPatch()));
}

bool ArchivePatchHelper::Uncompress(base::FilePath* last_uncompressed_file) {
  // The target shouldn't already exist.
  DCHECK(!base::PathExists(target_));

  // UnPackArchive takes care of logging.
  base::string16 output_file;
  int32 lzma_result = LzmaUtil::UnPackArchive(compressed_archive_.value(),
                                              working_directory_.value(),
                                              &output_file);
  if (lzma_result != NO_ERROR)
    return false;

  last_uncompressed_file_ = base::FilePath(output_file);
  if (last_uncompressed_file)
    *last_uncompressed_file = last_uncompressed_file_;
  return true;
}

bool ArchivePatchHelper::EnsemblePatch() {
  if (last_uncompressed_file_.empty()) {
    LOG(ERROR) << "No patch file found in compressed archive.";
    return false;
  }

  courgette::Status result =
      courgette::ApplyEnsemblePatch(patch_source_.value().c_str(),
                                    last_uncompressed_file_.value().c_str(),
                                    target_.value().c_str());
  if (result == courgette::C_OK)
    return true;

  LOG(ERROR)
      << "Failed to apply patch " << last_uncompressed_file_.value()
      << " to file " << patch_source_.value()
      << " and generating file " << target_.value()
      << " using courgette. err=" << result;

  // Ensure a partial output is not left behind.
  base::DeleteFile(target_, false);

  return false;
}

bool ArchivePatchHelper::BinaryPatch() {
  if (last_uncompressed_file_.empty()) {
    LOG(ERROR) << "No patch file found in compressed archive.";
    return false;
  }

  int result = ApplyBinaryPatch(patch_source_.value().c_str(),
                                last_uncompressed_file_.value().c_str(),
                                target_.value().c_str());
  if (result == OK)
    return true;

  LOG(ERROR)
      << "Failed to apply patch " << last_uncompressed_file_.value()
      << " to file " << patch_source_.value()
      << " and generating file " << target_.value()
      << " using bsdiff. err=" << result;

  // Ensure a partial output is not left behind.
  base::DeleteFile(target_, false);

  return false;
}

}  // namespace installer
