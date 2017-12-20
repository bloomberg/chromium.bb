// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"

namespace offline_items_collection {

namespace {

template <typename T, typename U>
bool MapContainsValue(const std::map<T, U>& map, U value) {
  for (const auto& it : map) {
    if (it.second == value)
      return true;
  }
  return false;
}

}  // namespace

OfflineContentAggregator::OfflineContentAggregator()
    : sent_on_items_available_(false), weak_ptr_factory_(this) {}

OfflineContentAggregator::~OfflineContentAggregator() = default;

void OfflineContentAggregator::RegisterProvider(
    const std::string& name_space,
    OfflineContentProvider* provider) {
  // Validate that this is the first OfflineContentProvider registered that is
  // associated with |name_space|.
  DCHECK(providers_.find(name_space) == providers_.end());

  // Only set up the connection to the provider if the provider isn't associated
  // with any other namespace.
  if (!MapContainsValue(providers_, provider))
    provider->AddObserver(this);

  providers_[name_space] = provider;
}

void OfflineContentAggregator::UnregisterProvider(
    const std::string& name_space) {
  auto provider_it = providers_.find(name_space);

  OfflineContentProvider* provider = provider_it->second;
  providers_.erase(provider_it);
  pending_providers_.erase(provider);

  // Only clean up the connection to the provider if the provider isn't
  // associated with any other namespace.
  if (!MapContainsValue(providers_, provider)) {
    provider->RemoveObserver(this);
    pending_actions_.erase(provider);
  }
}

bool OfflineContentAggregator::AreItemsAvailable() {
  return sent_on_items_available_;
}

void OfflineContentAggregator::OpenItem(const ContentId& id) {
  auto it = providers_.find(id.name_space);

  if (it == providers_.end())
    return;

  RunIfReady(it->second, base::Bind(&OfflineContentProvider::OpenItem,
                                    base::Unretained(it->second), id));
}

void OfflineContentAggregator::RemoveItem(const ContentId& id) {
  auto it = providers_.find(id.name_space);

  if (it == providers_.end())
    return;

  RunIfReady(it->second, base::Bind(&OfflineContentProvider::RemoveItem,
                                    base::Unretained(it->second), id));
}

void OfflineContentAggregator::CancelDownload(const ContentId& id) {
  auto it = providers_.find(id.name_space);

  if (it == providers_.end())
    return;

  RunIfReady(it->second, base::Bind(&OfflineContentProvider::CancelDownload,
                                    base::Unretained(it->second), id));
}

void OfflineContentAggregator::PauseDownload(const ContentId& id) {
  auto it = providers_.find(id.name_space);

  if (it == providers_.end())
    return;

  RunIfReady(it->second, base::Bind(&OfflineContentProvider::PauseDownload,
                                    base::Unretained(it->second), id));
}

void OfflineContentAggregator::ResumeDownload(const ContentId& id,
                                              bool has_user_gesture) {
  auto it = providers_.find(id.name_space);

  if (it == providers_.end())
    return;

  RunIfReady(it->second,
             base::Bind(&OfflineContentProvider::ResumeDownload,
                        base::Unretained(it->second), id, has_user_gesture));
}

void OfflineContentAggregator::GetItemById(const ContentId& id,
                                           SingleItemCallback callback) {
  auto it = providers_.find(id.name_space);
  if (it == providers_.end() || !it->second->AreItemsAvailable()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  it->second->GetItemById(
      id, base::BindOnce(&OfflineContentAggregator::OnGetItemByIdDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OfflineContentAggregator::OnGetItemByIdDone(
    SingleItemCallback callback,
    const base::Optional<OfflineItem>& item) {
  std::move(callback).Run(item);
}

void OfflineContentAggregator::GetAllItems(MultipleItemCallback callback) {
  // If there is already a call in progress, queue up the callback and wait for
  // the results.
  if (!multiple_item_get_callbacks_.empty()) {
    multiple_item_get_callbacks_.push_back(std::move(callback));
    return;
  }

  DCHECK(aggregated_items_.empty());
  for (auto provider_it : providers_) {
    auto* provider = provider_it.second;
    if (!provider->AreItemsAvailable())
      continue;

    provider->GetAllItems(
        base::BindOnce(&OfflineContentAggregator::OnGetAllItemsDone,
                       weak_ptr_factory_.GetWeakPtr(), provider));
    pending_providers_.insert(provider);
  }

  if (pending_providers_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), OfflineItemList()));
    return;
  }

  multiple_item_get_callbacks_.push_back(std::move(callback));
}

void OfflineContentAggregator::OnGetAllItemsDone(
    OfflineContentProvider* provider,
    const OfflineItemList& items) {
  aggregated_items_.insert(aggregated_items_.end(), items.begin(), items.end());
  pending_providers_.erase(provider);
  if (!pending_providers_.empty()) {
    return;
  }

  auto item_vec = std::move(aggregated_items_);
  auto callbacks = std::move(multiple_item_get_callbacks_);

  for (auto& callback : callbacks)
    std::move(callback).Run(item_vec);
}

void OfflineContentAggregator::GetVisualsForItem(
    const ContentId& id,
    const VisualsCallback& callback) {
  auto it = providers_.find(id.name_space);

  if (it == providers_.end()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, id, nullptr));
    return;
  }

  it->second->GetVisualsForItem(id, callback);
}

void OfflineContentAggregator::AddObserver(
    OfflineContentProvider::Observer* observer) {
  DCHECK(observer);
  if (observers_.HasObserver(observer))
    return;

  observers_.AddObserver(observer);

  if (sent_on_items_available_ || providers_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&OfflineContentAggregator::CheckAndNotifyItemsAvailable,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void OfflineContentAggregator::RemoveObserver(
    OfflineContentProvider::Observer* observer) {
  DCHECK(observer);
  if (!observers_.HasObserver(observer))
    return;

  signaled_observers_.erase(observer);
  observers_.RemoveObserver(observer);
}

void OfflineContentAggregator::NotifyItemsAdded(const OfflineItemList& items) {
  if (items.empty())
    return;

  for (auto* observer : signaled_observers_)
    observer->OnItemsAdded(items);
}

void OfflineContentAggregator::OnItemsAvailable(
    OfflineContentProvider* provider) {
  // Flush any pending actions that should be mirrored to the provider.
  FlushPendingActionsIfReady(provider);

  // Some observers might already be under the impression that this class was
  // initialized.  Just treat this as an OnItemsAdded and notify those observers
  // of the new items.
  if (signaled_observers_.size() > 0) {
    provider->GetAllItems(
        base::BindOnce(&OfflineContentAggregator::NotifyItemsAdded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Check if there were any observers who haven't been told that this class is
  // initialized yet.  If so, notify them now.
  CheckAndNotifyItemsAvailable();
}

void OfflineContentAggregator::OnItemsAdded(const OfflineItemList& items) {
  for (auto& observer : observers_)
    observer.OnItemsAdded(items);
}

void OfflineContentAggregator::OnItemRemoved(const ContentId& id) {
  for (auto& observer : observers_)
    observer.OnItemRemoved(id);
}

void OfflineContentAggregator::OnItemUpdated(const OfflineItem& item) {
  for (auto& observer : observers_)
    observer.OnItemUpdated(item);
}

void OfflineContentAggregator::CheckAndNotifyItemsAvailable() {
  // If we haven't sent out the initialization message yet, make sure all
  // underlying OfflineContentProviders are ready before notifying observers
  // that we're ready to send items.
  if (!sent_on_items_available_) {
    for (auto& it : providers_) {
      if (!it.second->AreItemsAvailable())
        return;
    }
  }

  // Notify all observers who haven't been told about the initialization that we
  // are initialized.  Track the observers so that we don't notify them again.
  for (auto& observer : observers_) {
    if (!base::ContainsKey(signaled_observers_, &observer)) {
      observer.OnItemsAvailable(this);
      signaled_observers_.insert(&observer);
    }
  }

  // Track that we've told the world that we are initialized.
  sent_on_items_available_ = true;
}

void OfflineContentAggregator::FlushPendingActionsIfReady(
    OfflineContentProvider* provider) {
  DCHECK(MapContainsValue(providers_, provider));

  if (!provider->AreItemsAvailable())
    return;

  CallbackList actions = std::move(pending_actions_[provider]);
  for (auto& action : actions) {
    action.Run();

    // Check to see if the OfflineContentProvider was removed during the call to
    // |action|.  If so stop the loop.
    if (pending_actions_.find(provider) == pending_actions_.end())
      return;
  }
}

void OfflineContentAggregator::RunIfReady(OfflineContentProvider* provider,
                                          const base::Closure& action) {
  if (provider->AreItemsAvailable())
    action.Run();
  else
    pending_actions_[provider].push_back(action);
}

}  // namespace offline_items_collection
