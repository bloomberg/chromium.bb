// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/download/download_util.h"

DownloadStatusUpdater::DownloadStatusUpdater() {
}

DownloadStatusUpdater::~DownloadStatusUpdater() {
  STLDeleteElements(&notifiers_);
}

bool DownloadStatusUpdater::GetProgress(float* progress,
                                        int* download_count) const {
  *progress = 0;
  *download_count = 0;
  bool progress_certain = true;
  int64 received_bytes = 0;
  int64 total_bytes = 0;

  for (std::vector<HyperbolicDownloadItemNotifier*>::const_iterator it =
       notifiers_.begin(); it != notifiers_.end(); ++it) {
    if ((*it)->GetManager()) {
      content::DownloadManager::DownloadVector items;
      (*it)->GetManager()->GetAllDownloads(&items);
      for (content::DownloadManager::DownloadVector::const_iterator it =
          items.begin(); it != items.end(); ++it) {
        if ((*it)->IsInProgress()) {
          ++*download_count;
          if ((*it)->GetTotalBytes() <= 0) {
            // We don't know how much more is coming down this pipe.
            progress_certain = false;
          } else {
            received_bytes += (*it)->GetReceivedBytes();
            total_bytes += (*it)->GetTotalBytes();
          }
        }
      }
    }
  }

  if (total_bytes > 0)
    *progress = static_cast<float>(received_bytes) / total_bytes;
  return progress_certain;
}

void DownloadStatusUpdater::AddManager(content::DownloadManager* manager) {
  notifiers_.push_back(new HyperbolicDownloadItemNotifier(manager, this));
  content::DownloadManager::DownloadVector items;
  manager->GetAllDownloads(&items);
  for (content::DownloadManager::DownloadVector::const_iterator
       it = items.begin(); it != items.end(); ++it) {
    if ((*it)->IsInProgress()) {
      UpdateAppIconDownloadProgress(*it);
      // Only need to notify once.
      break;
    }
  }
}

void DownloadStatusUpdater::OnDownloadCreated(
    content::DownloadManager* manager, content::DownloadItem* item) {
  UpdateAppIconDownloadProgress(item);
}

void DownloadStatusUpdater::OnDownloadUpdated(
    content::DownloadManager* manager, content::DownloadItem* item) {
  UpdateAppIconDownloadProgress(item);
}

#if defined(USE_AURA) || defined(OS_ANDROID)
void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    content::DownloadItem* download) {
  // TODO(davemoore): Implement once UX for aura download is decided <104742>
  // TODO(avi): Implement for Android?
}
#endif
