// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_taskified.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/model/add_page_task.h"
#include "components/offline_pages/core/model/create_archive_task.h"
#include "components/offline_pages/core/model/delete_page_task.h"
#include "components/offline_pages/core/model/get_pages_task.h"
#include "components/offline_pages/core/model/mark_page_accessed_task.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "url/gurl.h"

namespace offline_pages {

using ArchiverResult = OfflinePageArchiver::ArchiverResult;
using ClearStorageResult = ClearStorageTask::ClearStorageResult;

namespace {

const base::TimeDelta kClearStorageStartingDelay =
    base::TimeDelta::FromSeconds(20);
// The time that the storage cleanup will be triggered again since the last
// one.
const base::TimeDelta kClearStorageInterval = base::TimeDelta::FromMinutes(30);

void WrapInMultipleItemsCallback(const MultipleOfflineIdCallback& callback,
                                 const MultipleOfflinePageItemResult& pages) {
  std::vector<int64_t> results;
  for (const auto& page : pages)
    results.push_back(page.offline_id);
  callback.Run(results);
}

SavePageResult ArchiverResultToSavePageResult(ArchiverResult archiver_result) {
  switch (archiver_result) {
    case ArchiverResult::SUCCESSFULLY_CREATED:
      return SavePageResult::SUCCESS;
    case ArchiverResult::ERROR_DEVICE_FULL:
      return SavePageResult::DEVICE_FULL;
    case ArchiverResult::ERROR_CONTENT_UNAVAILABLE:
      return SavePageResult::CONTENT_UNAVAILABLE;
    case ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED:
      return SavePageResult::ARCHIVE_CREATION_FAILED;
    case ArchiverResult::ERROR_CANCELED:
      return SavePageResult::CANCELLED;
    case ArchiverResult::ERROR_SECURITY_CERTIFICATE:
      return SavePageResult::SECURITY_CERTIFICATE_ERROR;
    case ArchiverResult::ERROR_ERROR_PAGE:
      return SavePageResult::ERROR_PAGE;
    case ArchiverResult::ERROR_INTERSTITIAL_PAGE:
      return SavePageResult::INTERSTITIAL_PAGE;
    case ArchiverResult::ERROR_SKIPPED:
      return SavePageResult::SKIPPED;
    case ArchiverResult::ERROR_DIGEST_CALCULATION_FAILED:
      return SavePageResult::DIGEST_CALCULATION_FAILED;
  }
  NOTREACHED();
  return SavePageResult::CONTENT_UNAVAILABLE;
}

SavePageResult AddPageResultToSavePageResult(AddPageResult add_page_result) {
  switch (add_page_result) {
    case AddPageResult::SUCCESS:
      return SavePageResult::SUCCESS;
    case AddPageResult::ALREADY_EXISTS:
      return SavePageResult::ALREADY_EXISTS;
    case AddPageResult::STORE_FAILURE:
      return SavePageResult::STORE_FAILURE;
    case AddPageResult::RESULT_COUNT:
      break;
  }
  NOTREACHED();
  return SavePageResult::STORE_FAILURE;
}

}  // namespace

OfflinePageModelTaskified::OfflinePageModelTaskified(
    std::unique_ptr<OfflinePageMetadataStoreSQL> store,
    std::unique_ptr<ArchiveManager> archive_manager,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : store_(std::move(store)),
      archive_manager_(std::move(archive_manager)),
      policy_controller_(new ClientPolicyController()),
      task_queue_(this),
      weak_ptr_factory_(this) {
  CreateArchivesDirectoryIfNeeded();
  PostClearCachedPagesTask(true /* is_initializing */);
}

OfflinePageModelTaskified::~OfflinePageModelTaskified() {}

void OfflinePageModelTaskified::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OfflinePageModelTaskified::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OfflinePageModelTaskified::OnTaskQueueIsIdle() {}

void OfflinePageModelTaskified::SavePage(
    const SavePageParams& save_page_params,
    std::unique_ptr<OfflinePageArchiver> archiver,
    const SavePageCallback& callback) {
  auto task = base::MakeUnique<CreateArchiveTask>(
      GetArchiveDirectory(save_page_params.client_id.name_space),
      save_page_params, archiver.get(),
      base::Bind(&OfflinePageModelTaskified::OnCreateArchiveDone,
                 weak_ptr_factory_.GetWeakPtr(), callback));
  pending_archivers_.push_back(std::move(archiver));
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::AddPage(const OfflinePageItem& page,
                                        const AddPageCallback& callback) {
  auto task = base::MakeUnique<AddPageTask>(
      store_.get(), page,
      base::BindOnce(&OfflinePageModelTaskified::OnAddPageDone,
                     weak_ptr_factory_.GetWeakPtr(), page, callback));
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::MarkPageAccessed(int64_t offline_id) {
  auto task = base::MakeUnique<MarkPageAccessedTask>(store_.get(), offline_id,
                                                     base::Time::Now());
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::DeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {
  auto task = DeletePageTask::CreateTaskMatchingOfflineIds(
      store_.get(),
      base::BindOnce(&OfflinePageModelTaskified::OnDeleteDone,
                     weak_ptr_factory_.GetWeakPtr(), callback),
      offline_ids);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::DeletePagesByClientIds(
    const std::vector<ClientId>& client_ids,
    const DeletePageCallback& callback) {
  auto task = DeletePageTask::CreateTaskMatchingClientIds(
      store_.get(),
      base::BindOnce(&OfflinePageModelTaskified::OnDeleteDone,
                     weak_ptr_factory_.GetWeakPtr(), callback),
      client_ids);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::DeleteCachedPagesByURLPredicate(
    const UrlPredicate& predicate,
    const DeletePageCallback& callback) {
  auto task = DeletePageTask::CreateTaskMatchingUrlPredicateForCachedPages(
      store_.get(),
      base::BindOnce(&OfflinePageModelTaskified::OnDeleteDone,
                     weak_ptr_factory_.GetWeakPtr(), callback),
      policy_controller_.get(), predicate);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::GetAllPages(
    const MultipleOfflinePageItemCallback& callback) {
  auto task = GetPagesTask::CreateTaskMatchingAllPages(store_.get(), callback);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::GetPageByOfflineId(
    int64_t offline_id,
    const SingleOfflinePageItemCallback& callback) {
  auto task = GetPagesTask::CreateTaskMatchingOfflineId(store_.get(), callback,
                                                        offline_id);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::GetPagesByClientIds(
    const std::vector<ClientId>& client_ids,
    const MultipleOfflinePageItemCallback& callback) {
  auto task = GetPagesTask::CreateTaskMatchingClientIds(store_.get(), callback,
                                                        client_ids);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::GetPagesByURL(
    const GURL& url,
    URLSearchMode url_search_mode,
    const MultipleOfflinePageItemCallback& callback) {
  auto task = GetPagesTask::CreateTaskMatchingUrl(store_.get(), callback, url);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::GetPagesByNamespace(
    const std::string& name_space,
    const MultipleOfflinePageItemCallback& callback) {
  auto task = GetPagesTask::CreateTaskMatchingNamespace(store_.get(), callback,
                                                        name_space);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::GetPagesRemovedOnCacheReset(
    const MultipleOfflinePageItemCallback& callback) {
  auto task = GetPagesTask::CreateTaskMatchingPagesRemovedOnCacheReset(
      store_.get(), callback, policy_controller_.get());
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::GetPagesSupportedByDownloads(
    const MultipleOfflinePageItemCallback& callback) {
  auto task = GetPagesTask::CreateTaskMatchingPagesSupportedByDownloads(
      store_.get(), callback, policy_controller_.get());
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::GetPagesByRequestOrigin(
    const std::string& request_origin,
    const MultipleOfflinePageItemCallback& callback) {
  auto task = GetPagesTask::CreateTaskMatchingRequestOrigin(
      store_.get(), callback, request_origin);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::GetOfflineIdsForClientId(
    const ClientId& client_id,
    const MultipleOfflineIdCallback& callback) {
  // We're currently getting offline IDs by querying offline items based on
  // client ids, and then extract the offline IDs from the items. This is fine
  // since we're not expecting many pages with the same client ID.
  auto task = GetPagesTask::CreateTaskMatchingClientIds(
      store_.get(), base::Bind(&WrapInMultipleItemsCallback, callback),
      {client_id});
  task_queue_.AddTask(std::move(task));
}

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

// TODO(romax): see if this method can be moved into anonymous namespace after
// migrating UMAs.
void OfflinePageModelTaskified::InformSavePageDone(
    const SavePageCallback& callback,
    SavePageResult result,
    const OfflinePageItem& page) {
  if (result == SavePageResult::ARCHIVE_CREATION_FAILED)
    CreateArchivesDirectoryIfNeeded();
  if (!callback.is_null())
    callback.Run(result, page.offline_id);
}

void OfflinePageModelTaskified::OnCreateArchiveDone(
    const SavePageCallback& callback,
    OfflinePageItem proposed_page,
    OfflinePageArchiver* archiver,
    ArchiverResult archiver_result,
    const GURL& saved_url,
    const base::FilePath& file_path,
    const base::string16& title,
    int64_t file_size,
    const std::string& digest) {
  pending_archivers_.erase(
      std::find_if(pending_archivers_.begin(), pending_archivers_.end(),
                   [archiver](const std::unique_ptr<OfflinePageArchiver>& a) {
                     return a.get() == archiver;
                   }));

  if (archiver_result != ArchiverResult::SUCCESSFULLY_CREATED) {
    SavePageResult result = ArchiverResultToSavePageResult(archiver_result);
    InformSavePageDone(callback, result, proposed_page);
    return;
  }
  if (proposed_page.url != saved_url) {
    DVLOG(1) << "Saved URL does not match requested URL.";
    InformSavePageDone(callback, SavePageResult::ARCHIVE_CREATION_FAILED,
                       proposed_page);
    return;
  }
  proposed_page.file_path = file_path;
  proposed_page.file_size = file_size;
  proposed_page.title = title;
  AddPage(proposed_page,
          base::Bind(&OfflinePageModelTaskified::OnAddPageForSavePageDone,
                     weak_ptr_factory_.GetWeakPtr(), callback, proposed_page));
}

void OfflinePageModelTaskified::OnAddPageForSavePageDone(
    const SavePageCallback& callback,
    const OfflinePageItem& page_attempted,
    AddPageResult add_page_result,
    int64_t offline_id) {
  SavePageResult save_page_result =
      AddPageResultToSavePageResult(add_page_result);
  InformSavePageDone(callback, save_page_result, page_attempted);
  if (save_page_result == SavePageResult::SUCCESS)
    RemovePagesMatchingUrlAndNamespace(page_attempted);
  PostClearCachedPagesTask(false /* is_initializing */);
}

void OfflinePageModelTaskified::OnAddPageDone(const OfflinePageItem& page,
                                              const AddPageCallback& callback,
                                              AddPageResult result) {
  callback.Run(result, page.offline_id);
  if (result == AddPageResult::SUCCESS) {
    for (Observer& observer : observers_)
      observer.OfflinePageAdded(this, page);
  }
}

void OfflinePageModelTaskified::PostClearCachedPagesTask(bool is_initializing) {
  base::TimeDelta delay;
  if (is_initializing) {
    delay = kClearStorageStartingDelay;
  } else {
    if (base::Time::Now() - last_clear_cached_pages_time_ >
        kClearStorageInterval) {
      delay = base::TimeDelta();
    }
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&OfflinePageModelTaskified::ClearCachedPages,
                 weak_ptr_factory_.GetWeakPtr()),
      delay);
}

// TODO(romax): see if this method can be moved into anonymous namespace after
// migrating UMAs.
void OfflinePageModelTaskified::InformDeletePageDone(
    const DeletePageCallback& callback,
    DeletePageResult result) {
  if (!callback.is_null())
    callback.Run(result);
}

void OfflinePageModelTaskified::OnDeleteDone(
    const DeletePageCallback& callback,
    DeletePageResult result,
    const std::vector<OfflinePageModel::DeletedPageInfo>& infos) {
  for (const auto& info : infos) {
    for (Observer& observer : observers_)
      observer.OfflinePageDeleted(info);
  }
  InformDeletePageDone(callback, result);
}

void OfflinePageModelTaskified::ClearCachedPages() {
  auto task = base::MakeUnique<ClearStorageTask>(
      store_.get(), archive_manager_.get(), policy_controller_.get(),
      base::Time::Now(),
      base::BindOnce(&OfflinePageModelTaskified::OnClearCachedPagesDone,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now()));
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::OnClearCachedPagesDone(
    base::Time start_time,
    size_t deleted_page_count,
    ClearStorageResult result) {
  last_clear_cached_pages_time_ = start_time;
}

void OfflinePageModelTaskified::RemovePagesMatchingUrlAndNamespace(
    const OfflinePageItem& page) {
  auto task = DeletePageTask::CreateTaskDeletingForPageLimit(
      store_.get(),
      base::BindOnce(&OfflinePageModelTaskified::OnDeleteDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Bind([](DeletePageResult result) {})),
      policy_controller_.get(), page);
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::CreateArchivesDirectoryIfNeeded() {
  // No callback is required here.
  // TODO(romax): Remove the callback from the interface once the other
  // consumers of this API can also drop the callback.
  archive_manager_->EnsureArchivesDirCreated(base::Bind([]() {}));
}

}  // namespace offline_pages
