// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/test_support/mock_offline_content_provider.h"

namespace offline_items_collection {

MockOfflineContentProvider::MockObserver::MockObserver() = default;
MockOfflineContentProvider::MockObserver::~MockObserver() = default;

MockOfflineContentProvider::MockOfflineContentProvider()
    : items_available_(false) {}
MockOfflineContentProvider::~MockOfflineContentProvider() = default;

bool MockOfflineContentProvider::HasObserver(Observer* observer) {
  return observers_.HasObserver(observer);
}

void MockOfflineContentProvider::NotifyOnItemsAvailable() {
  items_available_ = true;
  for (auto& observer : observers_)
    observer.OnItemsAvailable(this);
}

void MockOfflineContentProvider::NotifyOnItemsAdded(
    const OfflineItemList& items) {
  for (auto& observer : observers_)
    observer.OnItemsAdded(items);
}

void MockOfflineContentProvider::NotifyOnItemRemoved(const ContentId& id) {
  for (auto& observer : observers_)
    observer.OnItemRemoved(id);
}

void MockOfflineContentProvider::NotifyOnItemUpdated(const OfflineItem& item) {
  for (auto& observer : observers_)
    observer.OnItemUpdated(item);
}

bool MockOfflineContentProvider::AreItemsAvailable() {
  return items_available_;
}

void MockOfflineContentProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockOfflineContentProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace offline_items_collection
