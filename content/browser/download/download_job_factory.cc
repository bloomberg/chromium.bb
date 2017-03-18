// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_job_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_job.h"
#include "content/browser/download/download_job_impl.h"
#include "content/browser/download/parallel_download_job.h"
#include "content/browser/download/parallel_download_utils.h"
#include "content/public/common/content_features.h"

namespace content {

std::unique_ptr<DownloadJob> DownloadJobFactory::CreateJob(
    DownloadItemImpl* download_item,
    std::unique_ptr<DownloadRequestHandleInterface> req_handle,
    const DownloadCreateInfo& create_info) {
  // Build parallel download job.
  if (ShouldUseParallelDownload(create_info)) {
    return base::MakeUnique<ParallelDownloadJob>(download_item,
                                                 std::move(req_handle),
                                                 create_info);
  }

  // An ordinary download job.
  return base::MakeUnique<DownloadJobImpl>(download_item,
                                           std::move(req_handle));
}

}  // namespace
