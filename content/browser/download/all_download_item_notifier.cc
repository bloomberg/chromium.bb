// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/all_download_item_notifier.h"

namespace content {

AllDownloadItemNotifier::AllDownloadItemNotifier(
    DownloadManager* manager,
    AllDownloadItemNotifier::Observer* observer)
    : manager_(manager), observer_(observer) {
  DCHECK(observer_);
  manager_->AddObserver(this);
  DownloadManager::DownloadVector items;
  manager_->GetAllDownloads(&items);
  for (DownloadManager::DownloadVector::const_iterator it = items.begin();
       it != items.end(); ++it) {
    (*it)->AddObserver(this);
    observing_.insert(*it);
  }
}

AllDownloadItemNotifier::~AllDownloadItemNotifier() {
  if (manager_)
    manager_->RemoveObserver(this);
  for (std::set<DownloadItem*>::const_iterator it = observing_.begin();
       it != observing_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  observing_.clear();
}

void AllDownloadItemNotifier::ManagerGoingDown(DownloadManager* manager) {
  DCHECK_EQ(manager_, manager);
  manager_->RemoveObserver(this);
  manager_ = NULL;
}

void AllDownloadItemNotifier::OnDownloadCreated(DownloadManager* manager,
                                                DownloadItem* item) {
  item->AddObserver(this);
  observing_.insert(item);
  observer_->OnDownloadCreated(manager, item);
}

void AllDownloadItemNotifier::OnDownloadUpdated(DownloadItem* item) {
  observer_->OnDownloadUpdated(manager_, item);
}

void AllDownloadItemNotifier::OnDownloadOpened(DownloadItem* item) {
  observer_->OnDownloadOpened(manager_, item);
}

void AllDownloadItemNotifier::OnDownloadRemoved(DownloadItem* item) {
  observer_->OnDownloadRemoved(manager_, item);
}

void AllDownloadItemNotifier::OnDownloadDestroyed(DownloadItem* item) {
  item->RemoveObserver(this);
  observing_.erase(item);
}

}  // namespace content
