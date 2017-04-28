// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/throttled_offline_content_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_items_collection/core/offline_item.h"

namespace offline_items_collection {
namespace {
const int kDelayBetweenUpdatesMs = 1000;
}  // namespace

ThrottledOfflineContentProvider::ThrottledOfflineContentProvider(
    OfflineContentProvider* provider)
    : ThrottledOfflineContentProvider(
          base::TimeDelta::FromMilliseconds(kDelayBetweenUpdatesMs),
          provider) {}

ThrottledOfflineContentProvider::ThrottledOfflineContentProvider(
    const base::TimeDelta& delay_between_updates,
    OfflineContentProvider* provider)
    : delay_between_updates_(delay_between_updates),
      last_update_time_(base::TimeTicks::Now()),
      update_queued_(false),
      wrapped_provider_(provider),
      weak_ptr_factory_(this) {
  DCHECK(wrapped_provider_);
  wrapped_provider_->AddObserver(this);
}

ThrottledOfflineContentProvider::~ThrottledOfflineContentProvider() {
  wrapped_provider_->RemoveObserver(this);
}

bool ThrottledOfflineContentProvider::AreItemsAvailable() {
  return wrapped_provider_->AreItemsAvailable();
}

void ThrottledOfflineContentProvider::OpenItem(const ContentId& id) {
  wrapped_provider_->OpenItem(id);
  FlushUpdates();
}

void ThrottledOfflineContentProvider::RemoveItem(const ContentId& id) {
  wrapped_provider_->RemoveItem(id);
  FlushUpdates();
}

void ThrottledOfflineContentProvider::CancelDownload(const ContentId& id) {
  wrapped_provider_->CancelDownload(id);
  FlushUpdates();
}

void ThrottledOfflineContentProvider::PauseDownload(const ContentId& id) {
  wrapped_provider_->PauseDownload(id);
  FlushUpdates();
}

void ThrottledOfflineContentProvider::ResumeDownload(const ContentId& id) {
  wrapped_provider_->ResumeDownload(id);
  FlushUpdates();
}

const OfflineItem* ThrottledOfflineContentProvider::GetItemById(
    const ContentId& id) {
  const OfflineItem* item = wrapped_provider_->GetItemById(id);
  if (item)
    UpdateItemIfPresent(*item);
  return item;
}

OfflineContentProvider::OfflineItemList
ThrottledOfflineContentProvider::GetAllItems() {
  OfflineItemList items = wrapped_provider_->GetAllItems();
  for (auto item : items)
    UpdateItemIfPresent(item);
  return items;
}

void ThrottledOfflineContentProvider::GetVisualsForItem(
    const ContentId& id,
    const VisualsCallback& callback) {
  wrapped_provider_->GetVisualsForItem(id, callback);
}

void ThrottledOfflineContentProvider::AddObserver(
    OfflineContentProvider::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (!wrapped_provider_->AreItemsAvailable())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ThrottledOfflineContentProvider::NotifyItemsAvailable,
                 weak_ptr_factory_.GetWeakPtr(), base::Unretained(observer)));
}

void ThrottledOfflineContentProvider::RemoveObserver(
    OfflineContentProvider::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ThrottledOfflineContentProvider::OnItemsAvailable(
    OfflineContentProvider* provider) {
  DCHECK_EQ(provider, wrapped_provider_);
  for (auto& observer : observers_)
    observer.OnItemsAvailable(this);
}

void ThrottledOfflineContentProvider::OnItemsAdded(
    const OfflineItemList& items) {
  for (auto& observer : observers_)
    observer.OnItemsAdded(items);
}

void ThrottledOfflineContentProvider::OnItemRemoved(const ContentId& id) {
  updates_.erase(id);
  for (auto& observer : observers_)
    observer.OnItemRemoved(id);
}

void ThrottledOfflineContentProvider::OnItemUpdated(const OfflineItem& item) {
  updates_[item.id] = item;

  // If we already queued an update, we're throttling, just wait until the
  // update passes through.
  if (update_queued_)
    return;

  // If we haven't sent an update recently, let the update go through.
  base::TimeDelta current_delay = base::TimeTicks::Now() - last_update_time_;
  if (current_delay >= delay_between_updates_) {
    FlushUpdates();
    return;
  }

  // Queue the update so we wait for the proper amount of time before notifying
  // observers.
  update_queued_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ThrottledOfflineContentProvider::FlushUpdates,
                 weak_ptr_factory_.GetWeakPtr()),
      delay_between_updates_ - current_delay);
}

void ThrottledOfflineContentProvider::NotifyItemsAvailable(
    OfflineContentProvider::Observer* observer) {
  if (!observers_.HasObserver(observer))
    return;
  observer->OnItemsAvailable(this);
}

void ThrottledOfflineContentProvider::UpdateItemIfPresent(
    const OfflineItem& item) {
  OfflineItemMap::iterator it = updates_.find(item.id);
  if (it != updates_.end())
    it->second = item;
}

void ThrottledOfflineContentProvider::FlushUpdates() {
  last_update_time_ = base::TimeTicks::Now();
  update_queued_ = false;

  OfflineItemMap updates = std::move(updates_);
  for (auto item_pair : updates) {
    for (auto& observer : observers_)
      observer.OnItemUpdated(item_pair.second);
  }
}

}  // namespace offline_items_collection
