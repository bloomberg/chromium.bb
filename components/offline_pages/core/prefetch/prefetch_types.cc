// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

RenderPageInfo::RenderPageInfo() = default;

RenderPageInfo::RenderPageInfo(const RenderPageInfo& other) = default;

PrefetchDownloadResult::PrefetchDownloadResult() = default;

PrefetchDownloadResult::PrefetchDownloadResult(const std::string& download_id,
                                               const base::FilePath& file_path,
                                               int64_t file_size)
    : download_id(download_id),
      success(true),
      file_path(file_path),
      file_size(file_size) {}

PrefetchDownloadResult::PrefetchDownloadResult(
    const PrefetchDownloadResult& other) = default;

bool PrefetchDownloadResult::operator==(
    const PrefetchDownloadResult& other) const {
  return download_id == other.download_id && success == other.success &&
         file_path == other.file_path && file_size == other.file_size;
}

PrefetchArchiveInfo::PrefetchArchiveInfo() = default;

PrefetchArchiveInfo::PrefetchArchiveInfo(const PrefetchArchiveInfo& other) =
    default;

PrefetchArchiveInfo::~PrefetchArchiveInfo() = default;

bool PrefetchArchiveInfo::empty() const {
  return offline_id == 0;
}

}  // namespace offline_pages
