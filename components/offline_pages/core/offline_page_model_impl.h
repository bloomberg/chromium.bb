// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/core/offline_page_archiver.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_model_event_logger.h"
#include "components/offline_pages/core/offline_page_storage_manager.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_types.h"

class GURL;
namespace base {
class Clock;
class SequencedTaskRunner;
class TimeTicks;
}  // namespace base

namespace offline_pages {

struct ClientId;
struct OfflinePageItem;

class ArchiveManager;
class ClientPolicyController;
class OfflinePageModelQuery;
class OfflinePageStorageManager;

// Implementation of service for saving pages offline, storing the offline
// copy and metadata, and retrieving them upon request.
class OfflinePageModelImpl : public OfflinePageModel {
 public:
  // All blocking calls/disk access will happen on the provided |task_runner|.
  OfflinePageModelImpl(
      std::unique_ptr<OfflinePageMetadataStore> store,
      std::unique_ptr<ArchiveManager> archive_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~OfflinePageModelImpl() override;

  // Implemented methods:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SavePage(const SavePageParams& save_page_params,
                std::unique_ptr<OfflinePageArchiver> archiver,
                const SavePageCallback& callback) override;
  void AddPage(const OfflinePageItem& page,
               const AddPageCallback& callback) override;
  void MarkPageAccessed(int64_t offline_id) override;
  void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                              const DeletePageCallback& callback) override;
  void DeletePagesByClientIds(const std::vector<ClientId>& client_ids,
                              const DeletePageCallback& callback) override;

  void GetPagesByClientIds(
      const std::vector<ClientId>& client_ids,
      const MultipleOfflinePageItemCallback& callback) override;

  void GetPagesByRequestOrigin(
      const std::string& request_origin,
      const MultipleOfflinePageItemCallback& callback) override;

  void DeleteCachedPagesByURLPredicate(
      const UrlPredicate& predicate,
      const DeletePageCallback& callback) override;
  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override;
  void GetOfflineIdsForClientId(
      const ClientId& client_id,
      const MultipleOfflineIdCallback& callback) override;
  void GetPageByOfflineId(
      int64_t offline_id,
      const SingleOfflinePageItemCallback& callback) override;
  void GetPagesByURL(
      const GURL& url,
      URLSearchMode url_search_mode,
      const MultipleOfflinePageItemCallback& callback) override;
  void GetPagesRemovedOnCacheReset(
      const MultipleOfflinePageItemCallback& callback) override;
  void GetPagesByNamespace(
      const std::string& name_space,
      const MultipleOfflinePageItemCallback& callback) override;
  void GetPagesSupportedByDownloads(
      const MultipleOfflinePageItemCallback& callback) override;
  const base::FilePath& GetInternalArchiveDirectory(
      const std::string& name_space) const override;
  bool IsArchiveInInternalDir(const base::FilePath& file_path) const override;

  ClientPolicyController* GetPolicyController() override;

  // Methods for testing only:
  OfflinePageMetadataStore* GetStoreForTesting();
  void set_testing_clock(base::Clock* clock) { testing_clock_ = clock; }

  OfflinePageStorageManager* GetStorageManager();

  OfflineEventLogger* GetLogger() override;

  void set_skip_clearing_original_url_for_testing() {
    skip_clearing_original_url_for_testing_ = true;
  }

 protected:
  // Adding a protected constructor for testing-only purposes in
  // offline_page_storage_manager_unittest.cc
  OfflinePageModelImpl();

 private:
  FRIEND_TEST_ALL_PREFIXES(OfflinePageModelImplTest, MarkPageForDeletion);
  FRIEND_TEST_ALL_PREFIXES(OfflinePageModelImplTest, StoreLoadFailurePersists);

  typedef std::vector<std::unique_ptr<OfflinePageArchiver>> PendingArchivers;

  // Callback for ensuring archive directory is created.
  void OnEnsureArchivesDirCreatedDone(const base::TimeTicks& start_time);

  void GetPagesMatchingQueryWhenLoadDone(
      std::unique_ptr<OfflinePageModelQuery> query,
      const MultipleOfflinePageItemCallback& callback);
  void GetOfflineIdsForClientIdWhenLoadDone(
      const ClientId& client_id,
      const MultipleOfflineIdCallback& callback) const;
  const std::vector<int64_t> MaybeGetOfflineIdsForClientId(
      const ClientId& client_id) const;
  void GetPagesByURLWhenLoadDone(
      const GURL& url,
      URLSearchMode url_search_mode,
      const MultipleOfflinePageItemCallback& callback) const;
  void MarkPageAccessedWhenLoadDone(int64_t offline_id);

  // Check the consistency between metadata store and archives on disk,
  // would delete the metadata entries which don't have an associated
  // archive and the archives which doesn't have a metadata in the store.
  // The expired pages (from previous versions) would also be cleared
  // during this process.
  void CheckMetadataConsistency();

  // Callback for loading pages from the offline page metadata store.
  void OnStoreInitialized(const base::TimeTicks& start_time,
                          int init_attempts_spent,
                          bool success);
  void RetryDbInitialization(const base::TimeTicks& start_time,
                             int init_attempts_spent);
  void OnInitialGetOfflinePagesDone(const base::TimeTicks& start_time,
                                    std::vector<OfflinePageItem> offline_pages);
  void FinalizeModelLoad();

  // Steps for saving a page offline.
  void OnCreateArchiveDone(const SavePageParams& save_page_params,
                           int64_t offline_id,
                           const base::Time& start_time,
                           const SavePageCallback& callback,
                           OfflinePageArchiver* archiver,
                           OfflinePageArchiver::ArchiverResult result,
                           const GURL& saved_url,
                           const base::FilePath& file_path,
                           const base::string16& title,
                           int64_t file_size,
                           const std::string& file_hash);
  void OnAddSavedPageDone(const OfflinePageItem& offline_page,
                          const SavePageCallback& callback,
                          AddPageResult add_result,
                          int64_t offline_id);
  void InformSavePageDone(const SavePageCallback& callback,
                          SavePageResult result,
                          const ClientId& client_id,
                          int64_t offline_id);
  void DeletePendingArchiver(OfflinePageArchiver* archiver);

  // Steps for adding a page entry to metadata store.
  void AddPageWhenLoadDone(const OfflinePageItem& page,
                           const AddPageCallback& callback);
  void OnAddPageDone(const OfflinePageItem& offline_page,
                     const AddPageCallback& callback,
                     ItemActionStatus status);

  // Steps for deleting files and data for an offline page.
  void OnDeleteArchiveFilesDone(const std::vector<int64_t>& offline_ids,
                                const DeletePageCallback& callback,
                                bool success);
  void OnRemoveOfflinePagesDone(
      const DeletePageCallback& callback,
      std::unique_ptr<OfflinePagesUpdateResult> result);
  void InformDeletePageDone(const DeletePageCallback& callback,
                            DeletePageResult result);

  void OnMarkPageAccesseDone(const OfflinePageItem& offline_page_item,
                             std::unique_ptr<OfflinePagesUpdateResult> result);

  // Callbacks for checking metadata consistency.
  void CheckMetadataConsistencyForArchivePaths(
      const std::set<base::FilePath>& archive_paths);
  // Methods that are executed during consistency check, including:
  // 1. Delete temporary pages which are in the abandoned cache directory.
  // 2. Delete pages without associated archive file from metadata store.
  // 3. Delete orphaned archive files without associated metadata from the disk.
  // And their corresponding callbacks after deletion finishes.
  void DeleteTemporaryPagesInAbandonedCacheDir();
  void DeletePagesMissingArchiveFile(
      const std::set<base::FilePath>& archive_paths);
  void OnDeletePagesMissingArchiveFileDone(
      const std::vector<int64_t>& offline_ids,
      DeletePageResult result);
  void DeleteOrphanedArchives(const std::set<base::FilePath>& archive_paths);
  void OnDeleteOrphanedArchivesDone(const std::vector<base::FilePath>& archives,
                                    bool success);

  // Callbacks for deleting pages with same URL when saving pages.
  void DeleteExistingPagesWithSameURL(const OfflinePageItem& offline_page);
  void OnPagesFoundWithSameURL(const OfflinePageItem& offline_page,
                               size_t pages_allowed,
                               const MultipleOfflinePageItemResult& items);
  void OnDeleteOldPagesWithSameURL(DeletePageResult result);

  void CacheLoadedData(const std::vector<OfflinePageItem>& offline_pages);

  // Actually does the work of deleting, requires the model is loaded.
  void DoDeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                                const DeletePageCallback& callback);

  // Actually does the work of deleting, requires the model is loaded.
  void DeletePages(const DeletePageCallback& callback,
                   const MultipleOfflinePageItemResult& items);

  void DoGetPagesByClientIds(const std::vector<ClientId>& client_ids,
                             const MultipleOfflinePageItemCallback& callback);

  // Similar to DoDeletePagesByOfflineId, does actual work of deleting, and
  // requires that the model is loaded.
  void DoDeleteCachedPagesByURLPredicate(const UrlPredicate& predicate,
                                         const DeletePageCallback& callback);

  // Clears expired pages if there are any or we're running out of storage.
  void ClearStorageIfNeeded(
      const OfflinePageStorageManager::ClearStorageCallback& callback);

  // Callback completing storage clearing.
  void OnStorageCleared(size_t cleared_page_count,
                        OfflinePageStorageManager::ClearStorageResult result);

  // Post task to clear storage.
  void PostClearStorageIfNeededTask(bool delayed);

  void RunWhenLoaded(const base::Closure& job);

  base::Time GetCurrentTime() const;

  // Persistent store for offline page metadata.
  std::unique_ptr<OfflinePageMetadataStore> store_;

  // The observers.
  base::ObserverList<Observer> observers_;

  bool is_loaded_;

  // In memory copy of the offline page metadata, keyed by offline IDs.
  std::map<int64_t, OfflinePageItem> offline_pages_;

  // Pending archivers owned by this model.
  PendingArchivers pending_archivers_;

  // Delayed tasks that should be invoked after the loading is done.
  std::vector<base::Closure> delayed_tasks_;

  // Controller of the client policies.
  std::unique_ptr<ClientPolicyController> policy_controller_;

  // Manager for the storage consumed by archives and responsible for
  // automatic page clearing.
  std::unique_ptr<OfflinePageStorageManager> storage_manager_;

  // Manager for the offline archive files and directory.
  std::unique_ptr<ArchiveManager> archive_manager_;

  // Logger to facilitate recording of events.
  OfflinePageModelEventLogger offline_event_logger_;

  // Clock for getting time in testing code. The setter is responsible to reset
  // it once it is not longer needed.
  base::Clock* testing_clock_;

  // Don't clear original URL if it is same as final URL. For testing only.
  bool skip_clearing_original_url_for_testing_;

  base::WeakPtrFactory<OfflinePageModelImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModelImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_IMPL_H_
