// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/installer_metrics.h"

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/strings/string_piece.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void BeginPersistentHistogramStorage() {
  base::GlobalHistogramAllocator::CreateWithLocalMemory(
      1 << 20,  // 1 MiB
      0,        // No identifier.
      installer::kSetupHistogramAllocatorName);
  base::GlobalHistogramAllocator::Get()->CreateTrackingHistograms(
      kSetupHistogramAllocatorName);

  // This can't be enabled until after the allocator is configured because
  // there is no other reporting out of setup other than persistent memory.
  base::HistogramBase::EnableActivityReportHistogram("setup");
}

void EndPersistentHistogramStorage(const base::FilePath& target_path) {
  base::PersistentHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  allocator->UpdateTrackingHistograms();

  base::FilePath file_path = target_path
      .AppendASCII(allocator->Name())
      .AddExtension(L".pma");

  base::StringPiece contents(static_cast<const char*>(allocator->data()),
                             allocator->used());
  if (base::ImportantFileWriter::WriteFileAtomically(file_path, contents))
    VLOG(1) << "Persistent histograms saved in file: " << file_path.value();
}

}  // namespace installer
