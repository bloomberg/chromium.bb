// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/downloads/download_ui_adapter.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/downloads/download_ui_item.h"
#include "components/offline_pages/offline_page_model.h"

namespace offline_pages {

namespace {
const char kDownloadUIAdapterKey[] = "download-ui-adapter";
}

DownloadUIAdapter::DownloadUIAdapter(OfflinePageModel* model)
    : model_(model),
      is_loaded_(false),
      weak_ptr_factory_(this) {
}

DownloadUIAdapter::~DownloadUIAdapter() { }

// static
DownloadUIAdapter* DownloadUIAdapter::FromOfflinePageModel(
    OfflinePageModel* offline_page_model) {
  DownloadUIAdapter* adapter = static_cast<DownloadUIAdapter*>(
      offline_page_model->GetUserData(kDownloadUIAdapterKey));
  if (!adapter) {
    adapter = new DownloadUIAdapter(offline_page_model);
    offline_page_model->SetUserData(kDownloadUIAdapterKey, adapter);
  }
  return adapter;
}

void DownloadUIAdapter::AddObserver(Observer* observer) {
  DCHECK(observer);
  if (!observers_.might_have_observers())
    LoadCache();
  observers_.AddObserver(observer);
  // If the items are already loaded, post the notification right away.
  // Don't just invoke it from here to avoid reentrancy in the client.
  if (is_loaded_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&DownloadUIAdapter::NotifyItemsLoaded,
                              weak_ptr_factory_.GetWeakPtr(),
                              base::Unretained(observer)));
  }
}

void DownloadUIAdapter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
  // Once the last observer is gone, clear cached data.
  if (!observers_.might_have_observers())
    ClearCache();
}

void DownloadUIAdapter::OfflinePageModelLoaded(OfflinePageModel* model) {
  // This signal is not used here.
}

void DownloadUIAdapter::OfflinePageModelChanged(OfflinePageModel* model) {
  DCHECK(model == model_);
  model_->GetAllPages(
      base::Bind(&DownloadUIAdapter::OnOfflinePagesChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DownloadUIAdapter::OfflinePageDeleted(
  int64_t offline_id, const ClientId& client_id) {
  for(const auto& item : items_) {
    if (item.second->guid == client_id.id) {
      items_.erase(item.first);
      FOR_EACH_OBSERVER(Observer, observers_, ItemDeleted(client_id.id));
      return;
    }
  }
}

const DownloadUIItemsMap& DownloadUIAdapter::GetAllItems() const {
  return items_;
}

const DownloadUIItem*
    DownloadUIAdapter::GetItem(const std::string& guid) const {
  DownloadUIItemsMap::const_iterator it = items_.find(guid);
  if (it == items_.end())
    return nullptr;
  return (*it).second.get();
}

void DownloadUIAdapter::LoadCache() {
  DCHECK(!is_loaded_);
  // TODO(dimich): Add fetching from RequestQueue as well.
  model_->AddObserver(this);
  model_->GetAllPages(
      base::Bind(&DownloadUIAdapter::OnOfflinePagesLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DownloadUIAdapter::ClearCache() {
  model_->RemoveObserver(this);
  items_.clear();
  is_loaded_ = false;
}

void DownloadUIAdapter::OnOfflinePagesLoaded(
    const MultipleOfflinePageItemResult& pages) {
  for (const auto& page : pages) {
    if (IsVisibleInUI(page)) {
      std::unique_ptr<DownloadUIItem> new_item(new DownloadUIItem(page));
      const std::string& guid = new_item->guid;
      DCHECK(items_.find(guid) == items_.end());
      items_[guid] = std::move(new_item);
    }
  }
  is_loaded_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, ItemsLoaded());
}

void DownloadUIAdapter::NotifyItemsLoaded(Observer* observer) {
  if (observer && observers_.HasObserver(observer))
    observer->ItemsLoaded();
}

// This method is only called by OPM when a single item added.
// TODO(dimich): change OPM to have real OnPageAdded/OnPageUpdated and
// simplify this code.
void DownloadUIAdapter::OnOfflinePagesChanged(
    const MultipleOfflinePageItemResult& pages) {
  std::vector<std::string> added_guids;
  for (const auto& page : pages) {
    if (!IsVisibleInUI(page))  // Item should be filtered out.
      continue;
    const std::string& guid = page.client_id.id;
    if (items_.find(guid) != items_.end())  // Item already exists.
      continue;
    std::unique_ptr<DownloadUIItem> item(new DownloadUIItem(page));
    items_[guid] = std::move(item);
    added_guids.push_back(guid);
  }
  // Only one added page
  CHECK(added_guids.size() <= 1);
  for (auto& guid : added_guids) {
    const DownloadUIItem& item = *(items_.find(guid)->second.get());
    FOR_EACH_OBSERVER(Observer, observers_, ItemAdded(item));
  }
}

bool DownloadUIAdapter::IsVisibleInUI(const OfflinePageItem& page) {
  // TODO(dimich): set up the right filter here.
  return page.client_id.name_space == kAsyncNamespace &&
         base::IsValidGUID(page.client_id.id);
}

}  // namespace offline_pages
