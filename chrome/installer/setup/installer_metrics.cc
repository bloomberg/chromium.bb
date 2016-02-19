// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/installer_metrics.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_persistence.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void BeginPersistentHistogramStorage() {
  base::SetPersistentHistogramMemoryAllocator(
      new base::LocalPersistentMemoryAllocator(
          1 << 20, 0,  // 1 MiB
          installer::kSetupHistogramAllocatorName));
  base::GetPersistentHistogramMemoryAllocator()->CreateTrackingHistograms(
      installer::kSetupHistogramAllocatorName);
}

void EndPersistentHistogramStorage(const base::FilePath& target_path) {
  // For atomicity, first write to a temporary file and then rename it.
  // The ImportantFileWriter would be good for this except it supports only
  // std::string for its data.
  base::PersistentMemoryAllocator* allocator =
      base::GetPersistentHistogramMemoryAllocator();
  allocator->UpdateTrackingHistograms();

  base::FilePath file_path = target_path
      .AppendASCII(allocator->Name())
      .AddExtension(L".pma");
  base::FilePath tmp_file_path;
  base::DeleteFile(file_path, false);

  if (base::CreateTemporaryFileInDir(file_path.DirName(), &tmp_file_path)) {
    // Allocator doesn't support more than 1GB so can never overflow.
    int used = static_cast<int>(allocator->used());
    if (base::WriteFile(tmp_file_path,
                        static_cast<const char*>(allocator->data()),
                        used) == used) {
      if (base::ReplaceFile(tmp_file_path, file_path, nullptr)) {
        VLOG(1) << "Persistent histograms saved in file: "
                << file_path.value();
      }
    }
    base::DeleteFile(tmp_file_path, false);
  }
}

}  // namespace installer
