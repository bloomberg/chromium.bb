// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_storage_manager.h"

#include "base/bind.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_client_policy.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"

using DeletePageResult = offline_pages::OfflinePageModel::DeletePageResult;
using MultipleOfflinePageItemResult =
    offline_pages::OfflinePageModel::MultipleOfflinePageItemResult;

namespace offline_pages {

OfflinePageStorageManager::OfflinePageStorageManager(OfflinePageModel* model)
    : model_(model),
      policy_controller_(model->GetPolicyController()),
      in_progress_(false),
      weak_ptr_factory_(this) {}

OfflinePageStorageManager::~OfflinePageStorageManager() {
}

void OfflinePageStorageManager::ClearPagesIfNeeded(
    const ClearPageCallback& callback) {
  if (!ShouldClearPages())
    return;
  in_progress_ = true;
  model_->GetAllPages(base::Bind(&OfflinePageStorageManager::ClearExpiredPages,
                                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void OfflinePageStorageManager::ClearExpiredPages(
    const ClearPageCallback& callback,
    const MultipleOfflinePageItemResult& pages) {
  DCHECK(in_progress_);
  std::vector<int64_t> offline_ids;
  GetExpiredPageIds(pages, offline_ids);
  model_->DeletePagesByOfflineId(
      offline_ids,
      base::Bind(&OfflinePageStorageManager::OnExpiredPagesDeleted,
                 weak_ptr_factory_.GetWeakPtr(), callback, offline_ids.size()));
}

void OfflinePageStorageManager::GetExpiredPageIds(
    const MultipleOfflinePageItemResult& pages,
    std::vector<int64_t>& offline_ids) {
  for (const auto& page : pages) {
    if (IsPageExpired(page))
      offline_ids.push_back(page.offline_id);
  }
}

void OfflinePageStorageManager::OnExpiredPagesDeleted(
    const ClearPageCallback& callback,
    int pages_cleared,
    DeletePageResult result) {
  in_progress_ = false;
  callback.Run(pages_cleared, result);
}

bool OfflinePageStorageManager::ShouldClearPages() {
  return !in_progress_ && model_->is_loaded();
}

bool OfflinePageStorageManager::IsPageExpired(const OfflinePageItem& page) {
  base::Time now = base::Time::Now();
  const LifetimePolicy& policy =
      policy_controller_->GetPolicy(page.client_id.name_space).lifetime_policy;
  return now - page.last_access_time > policy.expiration_period;
}

}  // namespace offline_pages
