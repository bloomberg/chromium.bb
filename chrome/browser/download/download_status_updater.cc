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
  for (std::set<content::DownloadManager*>::iterator it = managers_.begin();
       it != managers_.end(); ++it)
    (*it)->RemoveObserver(this);

  for (std::set<content::DownloadItem*>::iterator it = items_.begin();
       it != items_.end(); ++it)
    (*it)->RemoveObserver(this);
}

bool DownloadStatusUpdater::GetProgress(float* progress,
                                        int* download_count) const {
  *progress = 0;
  *download_count = items_.size();

  int64 received_bytes = 0;
  int64 total_bytes = 0;
  for (std::set<content::DownloadItem*>::const_iterator it = items_.begin();
       it != items_.end(); ++it) {
    if ((*it)->GetTotalBytes() <= 0)
      // We don't know how much more is coming down this pipe.
      return false;
    received_bytes += (*it)->GetReceivedBytes();
    total_bytes += (*it)->GetTotalBytes();
  }

  if (total_bytes > 0)
    *progress = static_cast<float>(received_bytes) / total_bytes;
  return true;
}

void DownloadStatusUpdater::AddManager(content::DownloadManager* manager) {
  DCHECK(!ContainsKey(managers_, manager));
  managers_.insert(manager);
  manager->AddObserver(this);
}

// Methods inherited from content::DownloadManager::Observer.
void DownloadStatusUpdater::ModelChanged(content::DownloadManager* manager) {
  std::vector<content::DownloadItem*> downloads;
  manager->GetAllDownloads(&downloads);

  std::vector<content::DownloadItem*> added_downloads;
  for (std::vector<content::DownloadItem*>::iterator it = downloads.begin();
       it != downloads.end(); ++it) {
    if (!(*it)->IsTemporary() && UpdateItem(*it))
      added_downloads.push_back(*it);
  }

  for (std::vector<content::DownloadItem*>::iterator it =
           added_downloads.begin();
       it != added_downloads.end();
       ++it) {
    UpdateAppIconDownloadProgress(*it);
  }
}

void DownloadStatusUpdater::ManagerGoingDown(
    content::DownloadManager* manager) {
  DCHECK(ContainsKey(managers_, manager));
  managers_.erase(manager);
  manager->RemoveObserver(this);
}

// Methods inherited from content::DownloadItem::Observer.
void DownloadStatusUpdater::OnDownloadUpdated(
    content::DownloadItem* download) {
  UpdateItem(download);
  UpdateAppIconDownloadProgress(download);
}

void DownloadStatusUpdater::OnDownloadDestroyed(
    content::DownloadItem* download) {
  if (ContainsKey(items_, download)) {
    items_.erase(download);
    download->RemoveObserver(this);
  }
  UpdateAppIconDownloadProgress(download);
}

void DownloadStatusUpdater::OnDownloadOpened(content::DownloadItem* download) {
}

#if defined(USE_AURA) || defined(OS_ANDROID)
void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    content::DownloadItem* download) {
  // TODO(davemoore): Implement once UX for aura download is decided <104742>
  // TODO(avi): Implement for Android?
}
#endif

// React to a transition that a download associated with one of our
// download managers has made.  Our goal is to have only IN_PROGRESS
// items on our set list, as they're the only ones that have relevance
// to GetProgress() return values.
bool DownloadStatusUpdater::UpdateItem(content::DownloadItem* download) {
  if (download->GetState() == content::DownloadItem::IN_PROGRESS) {
    if (!ContainsKey(items_, download)) {
      items_.insert(download);
      download->AddObserver(this);
      return true;
    }
  } else {
    if (ContainsKey(items_, download)) {
      items_.erase(download);
      download->RemoveObserver(this);
    }
  }

  return false;
}
