// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/downloads/download_ui_adapter.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/downloads/download_ui_item.h"
#include "components/offline_pages/core/offline_page_model.h"

namespace {
// Value of this constant doesn't matter, only its address is used.
const char kDownloadUIAdapterKey[] = "";
}

namespace offline_pages {

// static
DownloadUIAdapter* DownloadUIAdapter::FromOfflinePageModel(
    OfflinePageModel* model) {
  DCHECK(model);
  return static_cast<DownloadUIAdapter*>(
      model->GetUserData(kDownloadUIAdapterKey));
}

// static
void DownloadUIAdapter::AttachToOfflinePageModel(DownloadUIAdapter* adapter,
                                                 OfflinePageModel* model) {
  DCHECK(adapter);
  DCHECK(model);
  model->SetUserData(kDownloadUIAdapterKey, adapter);
}

DownloadUIAdapter::ItemInfo::ItemInfo(const OfflinePageItem& page,
                                      bool temporarily_hidden)
    : ui_item(base::MakeUnique<DownloadUIItem>(page)),
      is_request(false),
      offline_id(page.offline_id),
      client_id(page.client_id),
      temporarily_hidden(temporarily_hidden) {}

DownloadUIAdapter::ItemInfo::ItemInfo(const SavePageRequest& request,
                                      bool temporarily_hidden)
    : ui_item(base::MakeUnique<DownloadUIItem>(request)),
      is_request(true),
      offline_id(request.request_id()),
      client_id(request.client_id()),
      temporarily_hidden() {}

DownloadUIAdapter::ItemInfo::~ItemInfo() {}

DownloadUIAdapter::DownloadUIAdapter(OfflinePageModel* model,
                                     RequestCoordinator* request_coordinator,
                                     std::unique_ptr<Delegate> delegate)
    : model_(model),
      request_coordinator_(request_coordinator),
      delegate_(std::move(delegate)),
      state_(State::NOT_LOADED),
      observers_count_(0),
      weak_ptr_factory_(this) {
  delegate_->SetUIAdapter(this);
}

DownloadUIAdapter::~DownloadUIAdapter() {}

void DownloadUIAdapter::AddObserver(Observer* observer) {
  DCHECK(observer);
  if (observers_.HasObserver(observer))
    return;
  if (observers_count_ == 0)
    LoadCache();
  observers_.AddObserver(observer);
  ++observers_count_;
  // If the items are already loaded, post the notification right away.
  // Don't just invoke it from here to avoid reentrancy in the client.
  if (state_ == State::LOADED) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&DownloadUIAdapter::NotifyItemsLoaded,
                   weak_ptr_factory_.GetWeakPtr(), base::Unretained(observer)));
  }
}

void DownloadUIAdapter::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  if (!observers_.HasObserver(observer))
    return;
  observers_.RemoveObserver(observer);
  --observers_count_;
  // Once the last observer is gone, clear cached data.
  if (observers_count_ == 0)
    ClearCache();
}

void DownloadUIAdapter::OfflinePageModelLoaded(OfflinePageModel* model) {
  // This signal is not used here.
}

void DownloadUIAdapter::OfflinePageAdded(OfflinePageModel* model,
                                         const OfflinePageItem& added_page) {
  DCHECK(model == model_);
  if (!delegate_->IsVisibleInUI(added_page.client_id))
    return;

  bool temporarily_hidden =
      delegate_->IsTemporarilyHiddenInUI(added_page.client_id);
  AddItemHelper(base::MakeUnique<ItemInfo>(added_page, temporarily_hidden));
}

void DownloadUIAdapter::OfflinePageDeleted(int64_t offline_id,
                                           const ClientId& client_id) {
  if (!delegate_->IsVisibleInUI(client_id))
    return;
  DeleteItemHelper(client_id.id);
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnAdded(const SavePageRequest& added_request) {
  if (!delegate_->IsVisibleInUI(added_request.client_id()))
    return;

  bool temporarily_hidden =
      delegate_->IsTemporarilyHiddenInUI(added_request.client_id());
  AddItemHelper(base::MakeUnique<ItemInfo>(added_request, temporarily_hidden));
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnCompleted(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult status) {
  if (!delegate_->IsVisibleInUI(request.client_id()))
    return;

  // If request completed successfully, report ItemUpdated when a page is added
  // to the model. If the request failed, tell UI that the item is gone.
  if (status == RequestNotifier::BackgroundSavePageResult::SUCCESS)
    return;
  DeleteItemHelper(request.client_id().id);
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnChanged(const SavePageRequest& request) {
  if (!delegate_->IsVisibleInUI(request.client_id()))
    return;

  std::string guid = request.client_id().id;
  bool temporarily_hidden =
      delegate_->IsTemporarilyHiddenInUI(request.client_id());
  items_[guid] = base::MakeUnique<ItemInfo>(request, temporarily_hidden);

  if (state_ != State::LOADED)
    return;

  const DownloadUIItem& download_ui_item = *(items_[guid]->ui_item);
  for (Observer& observer : observers_)
    observer.ItemUpdated(download_ui_item);
}

void DownloadUIAdapter::OnNetworkProgress(const SavePageRequest& request,
                                          int64_t received_bytes) {
  for (auto& item : items_) {
    if (item.second->is_request &&
        item.second->offline_id == request.request_id()) {
      if (received_bytes == item.second->ui_item->download_progress_bytes)
        return;

      item.second->ui_item->download_progress_bytes = received_bytes;
      for (Observer& observer : observers_)
        observer.ItemUpdated(*(item.second->ui_item));
      return;
    }
  }
}

void DownloadUIAdapter::TemporaryHiddenStatusChanged(
    const ClientId& client_id) {
  bool hidden = delegate_->IsTemporarilyHiddenInUI(client_id);

  for (const auto& item : items_) {
    if (item.second->client_id == client_id) {
      if (item.second->temporarily_hidden == hidden)
        continue;
      item.second->temporarily_hidden = hidden;
      if (hidden) {
        for (Observer& observer : observers_)
          observer.ItemDeleted(item.second->ui_item->guid);
      } else {
        for (Observer& observer : observers_)
          observer.ItemAdded(*item.second->ui_item.get());
      }
    }
  }
}

std::vector<const DownloadUIItem*> DownloadUIAdapter::GetAllItems() const {
  std::vector<const DownloadUIItem*> result;
  for (const auto& item : items_) {
    if (delegate_->IsTemporarilyHiddenInUI(item.second->client_id))
      continue;
    result.push_back(item.second->ui_item.get());
  }
  return result;
}

const DownloadUIItem* DownloadUIAdapter::GetItem(
    const std::string& guid) const {
  DownloadUIItems::const_iterator it = items_.find(guid);
  if (it == items_.end() ||
      delegate_->IsTemporarilyHiddenInUI(it->second->client_id)) {
    return nullptr;
  }
  return it->second->ui_item.get();
}

void DownloadUIAdapter::DeleteItem(const std::string& guid) {
  DownloadUIItems::const_iterator it = items_.find(guid);
  if (it == items_.end())
    return;

  std::vector<int64_t> page_ids;
  page_ids.push_back(it->second->offline_id);
  model_->DeletePagesByOfflineId(
      page_ids, base::Bind(&DownloadUIAdapter::OnDeletePagesDone,
                           weak_ptr_factory_.GetWeakPtr()));
}

int64_t DownloadUIAdapter::GetOfflineIdByGuid(const std::string& guid) const {
  if (deleting_item_ && deleting_item_->ui_item->guid == guid)
    return deleting_item_->offline_id;

  DownloadUIItems::const_iterator it = items_.find(guid);
  if (it != items_.end())
    return it->second->offline_id;
  return 0;
}

// Note that several LoadCache calls may be issued before the async GetAllPages
// comes back.
void DownloadUIAdapter::LoadCache() {
  state_ = State::LOADING_PAGES;
  model_->GetAllPages(base::Bind(&DownloadUIAdapter::OnOfflinePagesLoaded,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void DownloadUIAdapter::ClearCache() {
  // Once loaded, this class starts to observe the model. Only remove observer
  // if it was added.
  if (state_ == State::LOADED) {
    model_->RemoveObserver(this);
    request_coordinator_->RemoveObserver(this);
  }
  items_.clear();
  state_ = State::NOT_LOADED;
}

void DownloadUIAdapter::OnOfflinePagesLoaded(
    const MultipleOfflinePageItemResult& pages) {
  // If multiple observers register quickly, the cache might be already loaded
  // by the previous LoadCache call. At the same time, if all observers already
  // left, there is no reason to populate the cache.
  if (state_ != State::LOADING_PAGES)
    return;
  for (const auto& page : pages) {
    if (delegate_->IsVisibleInUI(page.client_id)) {
      std::string guid = page.client_id.id;
      DCHECK(items_.find(guid) == items_.end());
      bool temporarily_hidden =
          delegate_->IsTemporarilyHiddenInUI(page.client_id);
      std::unique_ptr<ItemInfo> item =
          base::MakeUnique<ItemInfo>(page, temporarily_hidden);
      items_[guid] = std::move(item);
    }
  }
  model_->AddObserver(this);

  state_ = State::LOADING_REQUESTS;
  request_coordinator_->GetAllRequests(base::Bind(
      &DownloadUIAdapter::OnRequestsLoaded, weak_ptr_factory_.GetWeakPtr()));
}

void DownloadUIAdapter::OnRequestsLoaded(
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  // If multiple observers register quickly, the cache might be already loaded
  // by the previous LoadCache call. At the same time, if all observers already
  // left, there is no reason to populate the cache.
  if (state_ != State::LOADING_REQUESTS)
    return;

  for (const auto& request : requests) {
    if (delegate_->IsVisibleInUI(request->client_id())) {
      std::string guid = request->client_id().id;
      DCHECK(items_.find(guid) == items_.end());
      bool temporarily_hidden =
          delegate_->IsTemporarilyHiddenInUI(request->client_id());
      std::unique_ptr<ItemInfo> item =
          base::MakeUnique<ItemInfo>(*request.get(), temporarily_hidden);
      items_[guid] = std::move(item);
    }
  }
  request_coordinator_->AddObserver(this);

  state_ = State::LOADED;
  for (Observer& observer : observers_)
    observer.ItemsLoaded();
}

void DownloadUIAdapter::NotifyItemsLoaded(Observer* observer) {
  if (observer && observers_.HasObserver(observer))
    observer->ItemsLoaded();
}


void DownloadUIAdapter::OnDeletePagesDone(DeletePageResult result) {
  // TODO(dimich): Consider adding UMA to record user actions.
}

void DownloadUIAdapter::AddItemHelper(std::unique_ptr<ItemInfo> item_info) {
  const std::string& guid = item_info->ui_item->guid;

  DownloadUIItems::const_iterator it = items_.find(guid);
  // In case when request is completed and morphed into a page, this comes as
  // new page added and request completed. We ignore request completion
  // notification and when page is added, fire 'updated' instead of 'added'.
  bool request_to_page_transition =
      (it != items_.end() && it->second->is_request && !item_info->is_request);

  items_[guid] = std::move(item_info);

  if (state_ != State::LOADED)
    return;

  DownloadUIItem* download_ui_item = items_[guid]->ui_item.get();

  if (request_to_page_transition) {
    download_ui_item->download_state = DownloadUIItem::DownloadState::COMPLETE;
    if (!items_[guid]->temporarily_hidden) {
      for (Observer& observer : observers_)
        observer.ItemUpdated(*download_ui_item);
    }
  } else {
    if (!items_[guid]->temporarily_hidden) {
      for (Observer& observer : observers_)
        observer.ItemAdded(*download_ui_item);
    }
  }
}

void DownloadUIAdapter::DeleteItemHelper(const std::string& guid) {
  DownloadUIItems::iterator it = items_.find(guid);
  if (it == items_.end())
    return;
  DCHECK(deleting_item_ == nullptr);
  deleting_item_ = std::move(it->second);
  items_.erase(it);

  if (!deleting_item_->temporarily_hidden && state_ == State::LOADED) {
    for (Observer& observer : observers_)
      observer.ItemDeleted(guid);
  }

  deleting_item_.reset();
}

}  // namespace offline_pages
