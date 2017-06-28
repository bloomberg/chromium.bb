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
                                               uint64_t file_size)
    : download_id(download_id),
      success(true),
      file_path(file_path),
      file_size(file_size) {}

PrefetchDownloadResult::PrefetchDownloadResult(
    const PrefetchDownloadResult& other) = default;

}  // namespace offline_pages
