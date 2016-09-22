// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_test_store.h"

#include <map>

#include "base/bind.h"
#include "base/location.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

OfflinePageTestStore::OfflinePageTestStore(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner), scenario_(TestScenario::SUCCESSFUL) {}

OfflinePageTestStore::OfflinePageTestStore(
    const OfflinePageTestStore& other_store)
    : task_runner_(other_store.task_runner_),
      scenario_(other_store.scenario_),
      offline_pages_(other_store.offline_pages_) {}

OfflinePageTestStore::~OfflinePageTestStore() {}

void OfflinePageTestStore::GetOfflinePages(const LoadCallback& callback) {
  OfflinePageMetadataStore::LoadStatus load_status;
  if (scenario_ == TestScenario::LOAD_FAILED) {
    load_status = OfflinePageMetadataStore::STORE_LOAD_FAILED;
    offline_pages_.clear();
  } else {
    load_status = OfflinePageMetadataStore::LOAD_SUCCEEDED;
  }
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(callback, load_status, GetAllPages()));
}

void OfflinePageTestStore::AddOfflinePage(const OfflinePageItem& offline_page,
                                          const AddCallback& callback) {
  // TODO(fgorski): Add and cover scenario with existing item.
  ItemActionStatus result;
  if (scenario_ == TestScenario::SUCCESSFUL) {
    offline_pages_[offline_page.offline_id] = offline_page;
    last_saved_page_ = offline_page;
    result = ItemActionStatus::SUCCESS;
  } else if (scenario_ == TestScenario::WRITE_FAILED) {
    result = ItemActionStatus::STORE_ERROR;
  }
  if (!callback.is_null())
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, result));
}

void OfflinePageTestStore::UpdateOfflinePages(
    const std::vector<OfflinePageItem>& pages,
    const UpdateCallback& callback) {
  // TODO(fgorski): Cover scenario where new items are being created while they
  // shouldn't.
  std::unique_ptr<OfflinePagesUpdateResult> result(
      new OfflinePagesUpdateResult(StoreState::LOADED));
  if (scenario_ == TestScenario::WRITE_FAILED) {
    for (const auto& page : pages) {
      result->item_statuses.push_back(
          std::make_pair(page.offline_id, ItemActionStatus::STORE_ERROR));
    }
  } else {
    for (const auto& page : pages) {
      offline_pages_[page.offline_id] = page;
      last_saved_page_ = page;
      result->item_statuses.push_back(
          std::make_pair(page.offline_id, ItemActionStatus::SUCCESS));
    }
    result->updated_items.insert(result->updated_items.begin(), pages.begin(),
                                 pages.end());
  }
  if (!callback.is_null())
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(callback, base::Passed(&result)));
}

void OfflinePageTestStore::RemoveOfflinePages(
    const std::vector<int64_t>& offline_ids,
    const UpdateCallback& callback) {
  std::unique_ptr<OfflinePagesUpdateResult> result(
      new OfflinePagesUpdateResult(StoreState::LOADED));

  ASSERT_FALSE(offline_ids.empty());
  if (scenario_ == TestScenario::REMOVE_FAILED) {
    for (const auto& offline_id : offline_ids) {
      result->item_statuses.push_back(
          std::make_pair(offline_id, ItemActionStatus::STORE_ERROR));
    }
    // Anything different that LOADED is good here.
    result->store_state = StoreState::FAILED_LOADING;
  } else {
    for (const auto& offline_id : offline_ids) {
      auto iter = offline_pages_.find(offline_id);
      ItemActionStatus status;
      if (iter != offline_pages_.end()) {
        result->updated_items.push_back(iter->second);
        status = ItemActionStatus::SUCCESS;
        offline_pages_.erase(iter);
      } else {
        status = ItemActionStatus::NOT_FOUND;
      }
      result->item_statuses.push_back(std::make_pair(offline_id, status));
    }
  }

  task_runner_->PostTask(FROM_HERE,
                         base::Bind(callback, base::Passed(&result)));
}

void OfflinePageTestStore::Reset(const ResetCallback& callback) {
  offline_pages_.clear();
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, true));
}

StoreState OfflinePageTestStore::state() const {
  return StoreState::LOADED;
}

void OfflinePageTestStore::UpdateLastAccessTime(
    int64_t offline_id,
    const base::Time& last_access_time) {
  auto iter = offline_pages_.find(offline_id);
  if (iter == offline_pages_.end())
    return;
  iter->second.last_access_time = last_access_time;
}

std::vector<OfflinePageItem> OfflinePageTestStore::GetAllPages() const {
  std::vector<OfflinePageItem> offline_pages;
  for (const auto& id_page_pair : offline_pages_)
    offline_pages.push_back(id_page_pair.second);
  return offline_pages;
}

void OfflinePageTestStore::ClearAllPages() {
  offline_pages_.clear();
}

}  // namespace offline_pages
