// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/hyperbolic_download_item_notifier.h"

HyperbolicDownloadItemNotifier::HyperbolicDownloadItemNotifier(
    content::DownloadManager* manager,
    HyperbolicDownloadItemNotifier::Observer* observer)
    : manager_(manager),
      observer_(observer) {
  DCHECK(observer_);
  manager_->AddObserver(this);
  content::DownloadManager::DownloadVector items;
  manager_->GetAllDownloads(&items);
  for (content::DownloadManager::DownloadVector::const_iterator it =
         items.begin();
       it != items.end(); ++it) {
    (*it)->AddObserver(this);
    observing_.insert(*it);
  }
}

HyperbolicDownloadItemNotifier::~HyperbolicDownloadItemNotifier() {
  if (manager_)
    manager_->RemoveObserver(this);
  for (std::set<content::DownloadItem*>::const_iterator it =
          observing_.begin();
        it != observing_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  observing_.clear();
}

void HyperbolicDownloadItemNotifier::ManagerGoingDown(
    content::DownloadManager* manager) {
  DCHECK_EQ(manager_, manager);
  manager_->RemoveObserver(this);
  manager_ = NULL;
}

void HyperbolicDownloadItemNotifier::OnDownloadCreated(
    content::DownloadManager* manager,
    content::DownloadItem* item) {
  item->AddObserver(this);
  observing_.insert(item);
  observer_->OnDownloadCreated(manager, item);
}

void HyperbolicDownloadItemNotifier::OnDownloadUpdated(
    content::DownloadItem* item) {
  observer_->OnDownloadUpdated(manager_, item);
}

void HyperbolicDownloadItemNotifier::OnDownloadOpened(
    content::DownloadItem* item) {
  observer_->OnDownloadOpened(manager_, item);
}

void HyperbolicDownloadItemNotifier::OnDownloadRemoved(
    content::DownloadItem* item) {
  observer_->OnDownloadRemoved(manager_, item);
}

void HyperbolicDownloadItemNotifier::OnDownloadDestroyed(
    content::DownloadItem* item) {
  item->RemoveObserver(this);
  observing_.erase(item);
}
