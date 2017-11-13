// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/downloader/in_progress/in_progress_cache.h"

namespace download {

InProgressCache::InProgressCache(
    const base::FilePath& cache_file_path,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {}

InProgressCache::~InProgressCache() = default;

void InProgressCache::AddOrReplaceEntry(const DownloadEntry& entry) {
  // TODO(jming): Implementation.
}

DownloadEntry* InProgressCache::RetrieveEntry(const std::string& guid) {
  // TODO(jming): Implementation.
  return nullptr;
}

void InProgressCache::RemoveEntry(const std::string& guid) {
  // TODO(jming): Implementation.
}

}  // namespace download
