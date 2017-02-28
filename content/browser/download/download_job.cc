// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_job.h"

#include "content/browser/download/download_item_impl.h"

namespace content {

DownloadJob::DownloadJob(DownloadItemImpl* download_item)
    : download_item_(download_item), is_paused_(false) {}

DownloadJob::~DownloadJob() = default;

void DownloadJob::Pause() {
  is_paused_ = true;
}

void DownloadJob::Resume(bool resume_request) {
  is_paused_ = false;
}

void DownloadJob::StartDownload() const {
  download_item_->StartDownload();
}

}  // namespace content
