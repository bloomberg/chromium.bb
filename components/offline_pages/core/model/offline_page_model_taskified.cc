// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_taskified.h"

#include <memory>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "url/gurl.h"

namespace offline_pages {

OfflinePageModelTaskified::OfflinePageModelTaskified(
    std::unique_ptr<OfflinePageMetadataStoreSQL> store,
    std::unique_ptr<ArchiveManager> archive_manager,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : store_(std::move(store)),
      archive_manager_(std::move(archive_manager)),
      policy_controller_(new ClientPolicyController()),
      weak_ptr_factory_(this) {}

OfflinePageModelTaskified::~OfflinePageModelTaskified() {}

void OfflinePageModelTaskified::AddObserver(Observer* observer) {}

void OfflinePageModelTaskified::RemoveObserver(Observer* observer) {}

void OfflinePageModelTaskified::OnTaskQueueIsIdle() {}

void OfflinePageModelTaskified::SavePage(
    const SavePageParams& save_page_params,
    std::unique_ptr<OfflinePageArchiver> archiver,
    const SavePageCallback& callback) {}

void OfflinePageModelTaskified::AddPage(const OfflinePageItem& page,
                                        const AddPageCallback& callback) {}

void OfflinePageModelTaskified::MarkPageAccessed(int64_t offline_id) {}

void OfflinePageModelTaskified::DeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {}

void OfflinePageModelTaskified::DeletePagesByClientIds(
    const std::vector<ClientId>& client_ids,
    const DeletePageCallback& callback) {}

void OfflinePageModelTaskified::DeleteCachedPagesByURLPredicate(
    const UrlPredicate& predicate,
    const DeletePageCallback& callback) {}

void OfflinePageModelTaskified::GetAllPages(
    const MultipleOfflinePageItemCallback& callback) {}

void OfflinePageModelTaskified::GetPageByOfflineId(
    int64_t offline_id,
    const SingleOfflinePageItemCallback& callback) {}

void OfflinePageModelTaskified::GetPagesByClientIds(
    const std::vector<ClientId>& client_ids,
    const MultipleOfflinePageItemCallback& callback) {}

void OfflinePageModelTaskified::GetPagesByURL(
    const GURL& url,
    URLSearchMode url_search_mode,
    const MultipleOfflinePageItemCallback& callback) {}

void OfflinePageModelTaskified::GetPagesByNamespace(
    const std::string& name_space,
    const MultipleOfflinePageItemCallback& callback) {}

void OfflinePageModelTaskified::GetPagesRemovedOnCacheReset(
    const MultipleOfflinePageItemCallback& callback) {}

void OfflinePageModelTaskified::GetPagesSupportedByDownloads(
    const MultipleOfflinePageItemCallback& callback) {}

void OfflinePageModelTaskified::GetPagesByRequestOrigin(
    const std::string& request_origin,
    const MultipleOfflinePageItemCallback& callback) {}

void OfflinePageModelTaskified::GetOfflineIdsForClientId(
    const ClientId& client_id,
    const MultipleOfflineIdCallback& callback) {}

const base::FilePath& OfflinePageModelTaskified::GetArchiveDirectory(
    const std::string& name_space) const {
  if (policy_controller_->IsRemovedOnCacheReset(name_space))
    return archive_manager_->GetTemporaryArchivesDir();
  return archive_manager_->GetPersistentArchivesDir();
}

bool OfflinePageModelTaskified::is_loaded() const {
  NOTIMPLEMENTED();
  // TODO(romax): Remove the method after switch. No longer needed with
  // DB.Execute pattern.
  return false;
}

ClientPolicyController* OfflinePageModelTaskified::GetPolicyController() {
  return policy_controller_.get();
}

OfflineEventLogger* OfflinePageModelTaskified::GetLogger() {
  return &offline_event_logger_;
}

}  // namespace offline_pages
