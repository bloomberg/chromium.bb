// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_TASKIFIED_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_TASKIFIED_H_

#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_model_event_logger.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/core/task_queue.h"

class GURL;
namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

struct ClientId;
struct OfflinePageItem;

class ArchiveManager;
class ClientPolicyController;
class OfflinePageArchiver;
class OfflinePageMetadataStoreSQL;

// Implementaion of OfflinePageModel, which is a service for saving pages
// offline. It's an entry point to get information about Offline Pages and the
// base of related Offline Pages features.
// It owns a database which stores offline metadata, and uses TaskQueue for
// executing various tasks, including database operation or other process that
// needs to run on a background thread.
class OfflinePageModelTaskified : public OfflinePageModel,
                                  public KeyedService,
                                  public TaskQueue::Delegate {
 public:
  OfflinePageModelTaskified(
      std::unique_ptr<OfflinePageMetadataStoreSQL> store,
      std::unique_ptr<ArchiveManager> archive_manager,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~OfflinePageModelTaskified() override;

  // TaskQueue::Delegate implementation.
  void OnTaskQueueIsIdle() override;

  // OfflinePageModel implementation.
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
  void DeleteCachedPagesByURLPredicate(
      const UrlPredicate& predicate,
      const DeletePageCallback& callback) override;

  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override;
  void GetPageByOfflineId(
      int64_t offline_id,
      const SingleOfflinePageItemCallback& callback) override;
  void GetPagesByClientIds(
      const std::vector<ClientId>& client_ids,
      const MultipleOfflinePageItemCallback& callback) override;
  void GetPagesByURL(const GURL& url,
                     URLSearchMode url_search_mode,
                     const MultipleOfflinePageItemCallback& callback) override;
  void GetPagesByNamespace(
      const std::string& name_space,
      const MultipleOfflinePageItemCallback& callback) override;
  void GetPagesRemovedOnCacheReset(
      const MultipleOfflinePageItemCallback& callback) override;
  void GetPagesSupportedByDownloads(
      const MultipleOfflinePageItemCallback& callback) override;
  void GetPagesByRequestOrigin(
      const std::string& request_origin,
      const MultipleOfflinePageItemCallback& callback) override;

  void GetOfflineIdsForClientId(
      const ClientId& client_id,
      const MultipleOfflineIdCallback& callback) override;

  const base::FilePath& GetArchiveDirectory(
      const std::string& name_space) const override;

  ClientPolicyController* GetPolicyController() override;

  bool is_loaded() const override;

  OfflineEventLogger* GetLogger() override;

  // Methods for testing only:
  OfflinePageMetadataStoreSQL* GetStoreForTesting() { return store_.get(); }

 private:
  // Persistent store for offline page metadata.
  std::unique_ptr<OfflinePageMetadataStoreSQL> store_;

  // Manager for the offline archive files and directory.
  std::unique_ptr<ArchiveManager> archive_manager_;

  // Controller of the client policies.
  std::unique_ptr<ClientPolicyController> policy_controller_;

  // Logger to facilitate recording of events.
  OfflinePageModelEventLogger offline_event_logger_;

  base::WeakPtrFactory<OfflinePageModelTaskified> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModelTaskified);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_IMPL_TASKIFIED_H_
