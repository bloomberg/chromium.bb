// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/persistent_histogram_storage.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

PersistentHistogramStorage::PersistentHistogramStorage() {
  base::GlobalHistogramAllocator::CreateWithLocalMemory(
      1 << 20,  // 1 MiB
      0,        // No identifier.
      kSetupHistogramAllocatorName);
  base::GlobalHistogramAllocator::Get()->CreateTrackingHistograms(
      kSetupHistogramAllocatorName);
}

PersistentHistogramStorage::~PersistentHistogramStorage() {
  base::PersistentHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  allocator->UpdateTrackingHistograms();

  // Stop if |storage_dir_| isn't set or does not exist. That can happen if the
  // product hasn't been installed yet or if it has been uninstalled.
  if (storage_dir_.empty() || !base::DirectoryExists(storage_dir_))
    return;

  // Save data using the current time as the filename. The actual filename
  // doesn't matter (so long as it ends with the correct extension) but this
  // works as well as anything.
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  const base::FilePath file_path =
      storage_dir_
          .AppendASCII(base::StringPrintf("%04d%02d%02d%02d%02d%02d",
                                          exploded.year, exploded.month,
                                          exploded.day_of_month, exploded.hour,
                                          exploded.minute, exploded.second))
          .AddExtension(base::PersistentMemoryAllocator::kFileExtension);

  base::StringPiece contents(static_cast<const char*>(allocator->data()),
                             allocator->used());
  if (base::ImportantFileWriter::WriteFileAtomically(file_path, contents))
    VLOG(1) << "Persistent histograms saved in file: " << file_path.value();
}

// static
base::FilePath PersistentHistogramStorage::GetReportedStorageDir(
    const base::FilePath& target_path) {
  return target_path.AppendASCII(kSetupHistogramAllocatorName);
}

}  // namespace installer
