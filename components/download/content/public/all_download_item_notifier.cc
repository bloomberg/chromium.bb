// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/public/all_download_item_notifier.h"

#include "base/trace_event/memory_usage_estimator.h"

namespace download {

AllDownloadItemNotifier::AllDownloadItemNotifier(
    content::DownloadManager* manager)
    : manager_(manager) {
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

AllDownloadItemNotifier::~AllDownloadItemNotifier() {
  if (manager_)
    manager_->RemoveObserver(this);
  for (auto it = observing_.begin(); it != observing_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  observing_.clear();
}

size_t AllDownloadItemNotifier::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(observing_);
}

void AllDownloadItemNotifier::AddObserver(Observer* observer) {
  DCHECK(observer);
  if (observers_.HasObserver(observer))
    return;

  observers_.AddObserver(observer);
  if (manager_->IsManagerInitialized())
    observer->OnManagerInitialized(manager_);
}

void AllDownloadItemNotifier::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AllDownloadItemNotifier::OnManagerInitialized() {
  for (auto& observer : observers_)
    observer.OnManagerInitialized(manager_);
}

void AllDownloadItemNotifier::ManagerGoingDown(
    content::DownloadManager* manager) {
  DCHECK_EQ(manager_, manager);
  for (auto& observer : observers_)
    observer.OnManagerGoingDown(manager);

  manager_->RemoveObserver(this);
  manager_ = nullptr;
}

void AllDownloadItemNotifier::OnDownloadCreated(
    content::DownloadManager* manager,
    DownloadItem* item) {
  item->AddObserver(this);
  observing_.insert(item);
  for (auto& observer : observers_)
    observer.OnDownloadCreated(manager, item);
}

void AllDownloadItemNotifier::OnDownloadUpdated(DownloadItem* item) {
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(manager_, item);
}

void AllDownloadItemNotifier::OnDownloadOpened(DownloadItem* item) {
  for (auto& observer : observers_)
    observer.OnDownloadOpened(manager_, item);
}

void AllDownloadItemNotifier::OnDownloadRemoved(DownloadItem* item) {
  for (auto& observer : observers_)
    observer.OnDownloadRemoved(manager_, item);
}

void AllDownloadItemNotifier::OnDownloadDestroyed(DownloadItem* item) {
  item->RemoveObserver(this);
  observing_.erase(item);
}

}  // namespace download
