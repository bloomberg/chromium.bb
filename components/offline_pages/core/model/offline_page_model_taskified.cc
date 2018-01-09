// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_taskified.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/add_page_task.h"
#include "components/offline_pages/core/model/clear_legacy_temporary_pages_task.h"
#include "components/offline_pages/core/model/delete_page_task.h"
#include "components/offline_pages/core/model/get_pages_task.h"
#include "components/offline_pages/core/model/mark_page_accessed_task.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/model/persistent_pages_consistency_check_task.h"
#include "components/offline_pages/core/model/temporary_pages_consistency_check_task.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "url/gurl.h"

namespace offline_pages {

using ArchiverResult = OfflinePageArchiver::ArchiverResult;
using ClearStorageResult = ClearStorageTask::ClearStorageResult;

namespace {

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

void ReportPageHistogramAfterSuccessfulSaving(
    const OfflinePageItem& offline_page,
    const base::Time& save_time) {
  base::UmaHistogramCustomTimes(
      model_utils::AddHistogramSuffix(offline_page.client_id.name_space,
                                      "OfflinePages.SavePageTime"),
      save_time - offline_page.creation_time,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(10),
      50);

  base::UmaHistogramCustomCounts(
      model_utils::AddHistogramSuffix(offline_page.client_id.name_space,
                                      "OfflinePages.PageSize"),
      offline_page.file_size / 1024, 1, 10000, 50);
}

}  // namespace

// static
constexpr base::TimeDelta
    OfflinePageModelTaskified::kInitialUpgradeSelectionDelay;

// static
constexpr base::TimeDelta OfflinePageModelTaskified::kInitializingTaskDelay;

// static
constexpr base::TimeDelta OfflinePageModelTaskified::kClearStorageInterval;

OfflinePageModelTaskified::OfflinePageModelTaskified(
    std::unique_ptr<OfflinePageMetadataStoreSQL> store,
    std::unique_ptr<ArchiveManager> archive_manager,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    std::unique_ptr<base::Clock> clock)
    : store_(std::move(store)),
      archive_manager_(std::move(archive_manager)),
      policy_controller_(new ClientPolicyController()),
      clock_(std::move(clock)),
      task_queue_(this),
      skip_clearing_original_url_for_testing_(false),
      weak_ptr_factory_(this) {
  CreateArchivesDirectoryIfNeeded();
  PostClearLegacyTemporaryPagesTask();
  PostClearCachedPagesTask(true /* is_initializing */);
  PostCheckMetadataConsistencyTask(true /* is_initializing */);
  // TODO(fgorski): Call from here, when upgrade task is available:
  // PostSelectItemsMarkedForUpgrade();
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
  // Skip saving the page that is not intended to be saved, like local file
  // page.
  if (!OfflinePageModel::CanSaveURL(save_page_params.url)) {
    InformSavePageDone(callback, SavePageResult::SKIPPED,
                       save_page_params.client_id, kInvalidOfflineId);
    return;
  }

  // The web contents is not available if archiver is not created and passed.
  if (!archiver.get()) {
    InformSavePageDone(callback, SavePageResult::CONTENT_UNAVAILABLE,
                       save_page_params.client_id, kInvalidOfflineId);
    return;
  }

  // If we already have an offline id, use it.  If not, generate one.
  int64_t offline_id = save_page_params.proposed_offline_id;
  if (offline_id == kInvalidOfflineId)
    offline_id = store_utils::GenerateOfflineId();

  OfflinePageArchiver::CreateArchiveParams create_archive_params;
  // If the page is being saved in the background, we should try to remove the
  // popup overlay that obstructs viewing the normal content.
  create_archive_params.remove_popup_overlay = save_page_params.is_background;
  create_archive_params.use_page_problem_detectors =
      save_page_params.use_page_problem_detectors;
  archiver->CreateArchive(
      GetInternalArchiveDirectory(save_page_params.client_id.name_space),
      create_archive_params,
      base::Bind(&OfflinePageModelTaskified::OnCreateArchiveDone,
                 weak_ptr_factory_.GetWeakPtr(), save_page_params, offline_id,
                 GetCurrentTime(), callback));
  pending_archivers_.push_back(std::move(archiver));
}

void OfflinePageModelTaskified::AddPage(const OfflinePageItem& page,
                                        const AddPageCallback& callback) {
  auto task = std::make_unique<AddPageTask>(
      store_.get(), page,
      base::BindOnce(&OfflinePageModelTaskified::OnAddPageDone,
                     weak_ptr_factory_.GetWeakPtr(), page, callback));
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::MarkPageAccessed(int64_t offline_id) {
  auto task = std::make_unique<MarkPageAccessedTask>(store_.get(), offline_id,
                                                     GetCurrentTime());
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

const base::FilePath& OfflinePageModelTaskified::GetInternalArchiveDirectory(
    const std::string& name_space) const {
  if (policy_controller_->IsRemovedOnCacheReset(name_space))
    return archive_manager_->GetTemporaryArchivesDir();
  return archive_manager_->GetPrivateArchivesDir();
}

bool OfflinePageModelTaskified::IsArchiveInInternalDir(
    const base::FilePath& file_path) const {
  DCHECK(!file_path.empty());

  // TODO(jianli): Update this once persistent archives are moved into the
  // public directory.
  return archive_manager_->GetTemporaryArchivesDir().IsParent(file_path) ||
         archive_manager_->GetPrivateArchivesDir().IsParent(file_path);
}

ClientPolicyController* OfflinePageModelTaskified::GetPolicyController() {
  return policy_controller_.get();
}

OfflineEventLogger* OfflinePageModelTaskified::GetLogger() {
  return &offline_event_logger_;
}

void OfflinePageModelTaskified::InformSavePageDone(
    const SavePageCallback& callback,
    SavePageResult result,
    const ClientId& client_id,
    int64_t offline_id) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.SavePageCount",
                            model_utils::ToNamespaceEnum(client_id.name_space),
                            OfflinePagesNamespaceEnumeration::RESULT_COUNT);
  base::UmaHistogramEnumeration(
      model_utils::AddHistogramSuffix(client_id.name_space,
                                      "OfflinePages.SavePageResult"),
      result, SavePageResult::RESULT_COUNT);

  if (result == SavePageResult::ARCHIVE_CREATION_FAILED)
    CreateArchivesDirectoryIfNeeded();
  if (!callback.is_null())
    callback.Run(result, offline_id);
}

void OfflinePageModelTaskified::OnCreateArchiveDone(
    const SavePageParams& save_page_params,
    int64_t offline_id,
    const base::Time& start_time,
    const SavePageCallback& callback,
    OfflinePageArchiver* archiver,
    ArchiverResult archiver_result,
    const GURL& saved_url,
    const base::FilePath& file_path,
    const base::string16& title,
    int64_t file_size,
    const std::string& file_hash) {
  // Remove the |archiver| from the pending list once it completes creation.
  pending_archivers_.erase(
      std::find_if(pending_archivers_.begin(), pending_archivers_.end(),
                   [archiver](const std::unique_ptr<OfflinePageArchiver>& a) {
                     return a.get() == archiver;
                   }));

  if (archiver_result != ArchiverResult::SUCCESSFULLY_CREATED) {
    SavePageResult result = ArchiverResultToSavePageResult(archiver_result);
    InformSavePageDone(callback, result, save_page_params.client_id,
                       offline_id);
    return;
  }
  if (save_page_params.url != saved_url) {
    DVLOG(1) << "Saved URL does not match requested URL.";
    InformSavePageDone(callback, SavePageResult::ARCHIVE_CREATION_FAILED,
                       save_page_params.client_id, offline_id);
    return;
  }

  OfflinePageItem offline_page(saved_url, offline_id,
                               save_page_params.client_id, file_path, file_size,
                               start_time);
  offline_page.title = title;
  offline_page.digest = file_hash;
  offline_page.request_origin = save_page_params.request_origin;
  // Don't record the original URL if it is identical to the final URL. This is
  // because some websites might route the redirect finally back to itself upon
  // the completion of certain action, i.e., authentication, in the middle.
  if (skip_clearing_original_url_for_testing_ ||
      save_page_params.original_url != offline_page.url) {
    offline_page.original_url = save_page_params.original_url;
  }

  AddPage(offline_page,
          base::Bind(&OfflinePageModelTaskified::OnAddPageForSavePageDone,
                     weak_ptr_factory_.GetWeakPtr(), callback, offline_page));
}

void OfflinePageModelTaskified::OnAddPageForSavePageDone(
    const SavePageCallback& callback,
    const OfflinePageItem& page_attempted,
    AddPageResult add_page_result,
    int64_t offline_id) {
  SavePageResult save_page_result =
      AddPageResultToSavePageResult(add_page_result);
  InformSavePageDone(callback, save_page_result, page_attempted.client_id,
                     offline_id);
  if (save_page_result == SavePageResult::SUCCESS) {
    ReportPageHistogramAfterSuccessfulSaving(page_attempted, GetCurrentTime());
    // TODO(romax): Just keep the same with logic in OPMImpl (which was wrong).
    // This should be fixed once we have the new strategy for clearing pages.
    if (policy_controller_->GetPolicy(page_attempted.client_id.name_space)
            .pages_allowed_per_url != kUnlimitedPages) {
      RemovePagesMatchingUrlAndNamespace(page_attempted);
      PostClearCachedPagesTask(false /* is_initializing */);
    }
  } else {
    PostClearCachedPagesTask(false /* is_initializing */);
  }
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

void OfflinePageModelTaskified::OnDeleteDone(
    const DeletePageCallback& callback,
    DeletePageResult result,
    const std::vector<OfflinePageModel::DeletedPageInfo>& infos) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.DeletePageResult", result,
                            DeletePageResult::RESULT_COUNT);

  // Notify observers and run callback.
  for (const auto& info : infos) {
    UMA_HISTOGRAM_ENUMERATION(
        "OfflinePages.DeletePageCount",
        model_utils::ToNamespaceEnum(info.client_id.name_space),
        OfflinePagesNamespaceEnumeration::RESULT_COUNT);
    for (Observer& observer : observers_)
      observer.OfflinePageDeleted(info);
  }
  if (!callback.is_null())
    callback.Run(result);
}

void OfflinePageModelTaskified::PostClearLegacyTemporaryPagesTask() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&OfflinePageModelTaskified::ClearLegacyTemporaryPages,
                 weak_ptr_factory_.GetWeakPtr()),
      kInitializingTaskDelay);
}

void OfflinePageModelTaskified::ClearLegacyTemporaryPages() {
  // TODO(romax): When we have external directory, adding the support of getting
  // 'legacy' directory and replace the persistent one here.
  auto task = std::make_unique<ClearLegacyTemporaryPagesTask>(
      store_.get(), policy_controller_.get(),
      archive_manager_->GetPrivateArchivesDir());
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::PostClearCachedPagesTask(bool is_initializing) {
  if (is_initializing) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&OfflinePageModelTaskified::PostClearCachedPagesTask,
                   weak_ptr_factory_.GetWeakPtr(), false),
        kInitializingTaskDelay);
  }

  // If not enough time has passed, do not post the task.
  if (GetCurrentTime() - last_clear_cached_pages_time_ <
      kClearStorageInterval) {
    return;
  }

  ClearCachedPages();
}

void OfflinePageModelTaskified::ClearCachedPages() {
  auto task = std::make_unique<ClearStorageTask>(
      store_.get(), archive_manager_.get(), policy_controller_.get(),
      GetCurrentTime(),
      base::BindOnce(&OfflinePageModelTaskified::OnClearCachedPagesDone,
                     weak_ptr_factory_.GetWeakPtr(), GetCurrentTime()));
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::OnClearCachedPagesDone(
    base::Time start_time,
    size_t deleted_page_count,
    ClearStorageResult result) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.ClearTemporaryPages.Result", result,
                            ClearStorageResult::RESULT_COUNT);
  if (deleted_page_count > 0) {
    UMA_HISTOGRAM_COUNTS("OfflinePages.ClearTemporaryPages.BatchSize",
                         deleted_page_count);
  }
  last_clear_cached_pages_time_ = start_time;
}

void OfflinePageModelTaskified::PostCheckMetadataConsistencyTask(
    bool is_initializing) {
  if (is_initializing) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&OfflinePageModelTaskified::PostCheckMetadataConsistencyTask,
                   weak_ptr_factory_.GetWeakPtr(), false),
        kInitializingTaskDelay);
    return;
  }

  CheckTemporaryPagesConsistency();
  CheckPersistentPagesConsistency();
}

void OfflinePageModelTaskified::CheckTemporaryPagesConsistency() {
  auto task = std::make_unique<TemporaryPagesConsistencyCheckTask>(
      store_.get(), policy_controller_.get(),
      archive_manager_->GetTemporaryArchivesDir());
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::CheckPersistentPagesConsistency() {
  auto task = std::make_unique<PersistentPagesConsistencyCheckTask>(
      store_.get(), policy_controller_.get(),
      archive_manager_->GetPrivateArchivesDir());
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::PostSelectItemsMarkedForUpgrade() {
  // TODO(fgorski): Make storage permission check. Here or later?
  // TODO(fgorski): Check disk space here.
  if (!IsOfflinePagesSharingEnabled())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindRepeating(
          &OfflinePageModelTaskified::SelectItemsMarkedForUpgrade,
          weak_ptr_factory_.GetWeakPtr()),
      kInitialUpgradeSelectionDelay);
}

void OfflinePageModelTaskified::SelectItemsMarkedForUpgrade() {
  // TODO(fgorski): Add legacy Persistent path in archive manager to know which
  // files still need upgrade.
  auto task = GetPagesTask::CreateTaskSelectingItemsMarkedForUpgrade(
      store_.get(),
      base::BindRepeating(
          &OfflinePageModelTaskified::OnSelectItemsMarkedForUpgradeDone,
          weak_ptr_factory_.GetWeakPtr()));
  task_queue_.AddTask(std::move(task));
}

void OfflinePageModelTaskified::OnSelectItemsMarkedForUpgradeDone(
    const MultipleOfflinePageItemResult& pages_for_upgrade) {
  // TODO(fgorski): Save the list of ID to feed them into the upgrade task.
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

base::Time OfflinePageModelTaskified::GetCurrentTime() {
  CHECK(clock_);
  return clock_->Now();
}

}  // namespace offline_pages
