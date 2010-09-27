// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include "base/logging.h"
#include "chrome/browser/download/download_status_updater_delegate.h"
#include "chrome/browser/download/download_util.h"

DownloadStatusUpdater::DownloadStatusUpdater() {
}

DownloadStatusUpdater::~DownloadStatusUpdater() {
  DCHECK(delegates_.empty());
}

void DownloadStatusUpdater::AddDelegate(
    DownloadStatusUpdaterDelegate* delegate) {
  delegates_.insert(delegate);
}

void DownloadStatusUpdater::RemoveDelegate(
    DownloadStatusUpdaterDelegate* delegate) {
  delegates_.erase(delegate);
}

void DownloadStatusUpdater::Update() {
  float progress = 0;
  bool progress_known = GetProgress(&progress);
  download_util::UpdateAppIconDownloadProgress(
      static_cast<int>(GetInProgressDownloadCount()),
      progress_known,
      progress);
}

bool DownloadStatusUpdater::GetProgress(float* progress) {
  *progress = 0;

  bool progress_known = true;
  int64 received_bytes = 0;
  int64 total_bytes = 0;
  for (DelegateSet::iterator i = delegates_.begin();
       i != delegates_.end(); ++i) {
    progress_known = progress_known && (*i)->IsDownloadProgressKnown();
    received_bytes += (*i)->GetReceivedDownloadBytes();
    total_bytes += (*i)->GetTotalDownloadBytes();
  }

  if (!progress_known)
    return false;

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
