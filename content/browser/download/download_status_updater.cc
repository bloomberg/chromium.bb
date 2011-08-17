// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_status_updater.h"

#include "base/logging.h"
#include "content/browser/download/download_status_updater_delegate.h"

DownloadStatusUpdater::DownloadStatusUpdater() {
}

DownloadStatusUpdater::~DownloadStatusUpdater() {
}

void DownloadStatusUpdater::AddDelegate(
    DownloadStatusUpdaterDelegate* delegate) {
  delegates_.insert(delegate);
}

void DownloadStatusUpdater::RemoveDelegate(
    DownloadStatusUpdaterDelegate* delegate) {
  delegates_.erase(delegate);
}

bool DownloadStatusUpdater::GetProgress(float* progress,
                                        int* download_count) {
  *progress = 0;
  *download_count = GetInProgressDownloadCount();

  int64 received_bytes = 0;
  int64 total_bytes = 0;
  for (DelegateSet::iterator i = delegates_.begin();
       i != delegates_.end(); ++i) {
    if (!(*i)->IsDownloadProgressKnown())
      return false;
    received_bytes += (*i)->GetReceivedDownloadBytes();
    total_bytes += (*i)->GetTotalDownloadBytes();
  }

  if (total_bytes > 0)
    *progress = static_cast<float>(received_bytes) / total_bytes;
  return true;
}

int64 DownloadStatusUpdater::GetInProgressDownloadCount() {
  int64 download_count = 0;
  for (DelegateSet::iterator i = delegates_.begin();
       i != delegates_.end(); ++i) {
    download_count += (*i)->GetInProgressDownloadCount();
  }

  return download_count;
}
