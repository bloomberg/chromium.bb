// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_package_download_job.h"

namespace content {

SavePackageDownloadJob::SavePackageDownloadJob(
    DownloadItemImpl* download_item,
    std::unique_ptr<DownloadRequestHandleInterface> request_handle)
    : DownloadJob(download_item, std::move(request_handle)) {}

SavePackageDownloadJob::~SavePackageDownloadJob() = default;

bool SavePackageDownloadJob::IsSavePackageDownload() const {
  return true;
}

}  // namespace content
