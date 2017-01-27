// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_job.h"

namespace content {

// Unknown download progress.
const int kDownloadProgressUnknown = -1;

// Unknown download speed.
const int kDownloadSpeedUnknown = -1;

DownloadJob::DownloadJob() : manager_(nullptr) {}

DownloadJob::~DownloadJob() = default;

void DownloadJob::OnAttached(DownloadJob::Manager* manager) {
  DCHECK(!manager_) << "DownloadJob::Manager has already been attached.";
  manager_ = manager;
}

void DownloadJob::OnBeforeDetach() {
  manager_ = nullptr;
}

void DownloadJob::StartedSavingResponse() {
  if (manager_)
    manager_->OnSavingStarted(this);
}

void DownloadJob::Interrupt(DownloadInterruptReason reason) {
  if (manager_)
    manager_->OnDownloadInterrupted(this, reason);
}

void DownloadJob::Complete() {
  if (manager_)
    manager_->OnDownloadComplete(this);
}

}  // namespace content
