// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mock_download_job.h"

namespace content {

MockDownloadJob::MockDownloadJob(download::DownloadItem* download_item)
    : download::DownloadJob(download_item, nullptr) {}

MockDownloadJob::~MockDownloadJob() = default;

}  // namespace content
