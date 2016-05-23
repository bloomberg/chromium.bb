// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_STORAGE_MANAGER_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_STORAGE_MANAGER_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/offline_pages/archive_manager.h"
#include "components/offline_pages/offline_page_types.h"

namespace base {
class Clock;
}  // namespace base

namespace offline_pages {

// Limit of the total storage space occupied by offline pages should be 30% of
// available storage. And we clear storage when it is over the threshold,
// reducing the usage below threshold.
const double kOfflinePageStorageLimit = 0.3;
const double kOfflinePageStorageClearThreshold = 0.1;
const base::TimeDelta kClearStorageInterval = base::TimeDelta::FromMinutes(10);

class ArchiveManager;
class ClientPolicyController;

// This class is used for storage management of offline pages. It provides
// a ClearPagesIfNeeded method which is used to clear expired offline pages
// based on last_access_time and lifetime policy of its namespace.
// It has its own throttle mechanism so calling the method would not be
// guaranteed to clear the pages immediately.
//
// OfflinePageModel should own and control the lifecycle of this manager.
// And this manager would use OfflinePageModel to get/remove pages.
class OfflinePageStorageManager {
 public:
  // This interface should have no knowledge of offline page model.
  // This interface should be implemented by clients managed by storage manager.
  class Client {
   public:
    virtual ~Client() {}

    // Asks the client to get all offline pages and invoke |callback|.
    virtual void GetAllPages(
        const MultipleOfflinePageItemCallback& callback) = 0;

    // Asks the client to delete pages based on |offline_ids| and invoke
    // |callback|.
    virtual void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                                        const DeletePageCallback& callback) = 0;
  };

  enum class ClearStorageResult {
    SUCCESS,         // Cleared successfully.
    UNNECESSARY,     // No expired pages.
    DELETE_FAILURE,  // Deletion failed.
  };

  // Callback used when calling ClearPagesIfNeeded.
  // int: the number of expired pages.
  // ClearStorageResult: result of expiring pages in storage.
  typedef base::Callback<void(int, ClearStorageResult)> ClearPagesCallback;

  explicit OfflinePageStorageManager(Client* client,
                                     ClientPolicyController* policy_controller,
                                     ArchiveManager* archive_manager);

  ~OfflinePageStorageManager();

  // The manager would *try* to clear pages when called. It may not delete any
  // pages (if clearing condition wasn't satisfied).
  // It clears the storage (expire pages) when it's using more disk space than a
  // certain limit, or the time elapsed from last time clearing is longer than a
  // certain interval. Both values are defined above.
  void ClearPagesIfNeeded(const ClearPagesCallback& callback);

  // Sets the clock for testing.
  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

 private:
  // Enum indicating how to clear the pages.
  enum class ClearMode {
    // Using normal expiration logic to expire pages. Will reduce the storage
    // usage down below the threshold.
    DEFAULT,
    // No need to expire any page (no pages in the model or no expired
    // pages and we're not exceeding the storage limit.)
    NOT_NEEDED,
  };

  // Callback called after getting storage stats from archive manager.
  void OnGetStorageStatsDone(const ClearPagesCallback& callback,
                             const ArchiveManager::StorageStats& pages);

  // Callback called after getting all pages from client done.
  void OnGetAllPagesDone(const ClearPagesCallback& callback,
                         const ArchiveManager::StorageStats& storage_stats,
                         const MultipleOfflinePageItemResult& pages);

  // Callback called after expired pages have been deleted.
  void OnExpiredPagesDeleted(const ClearPagesCallback& callback,
                             int pages_to_clear,
                             DeletePageResult result);

  // Gets offline IDs of all expired pages and return in |offline_ids|.
  void GetExpiredPageIds(const MultipleOfflinePageItemResult& pages,
                         const ArchiveManager::StorageStats& stats,
                         std::vector<int64_t>& offline_ids);

  // Determine if manager should clear pages.
  ClearMode ShouldClearPages(const ArchiveManager::StorageStats& storage_stats);

  // Return true if |page| is expired comparing to |now|.
  bool ShouldBeExpired(const base::Time& now, const OfflinePageItem& page);

  // Not owned.
  Client* client_;

  // Not owned.
  ClientPolicyController* policy_controller_;

  // Not owned.
  ArchiveManager* archive_manager_;

  bool in_progress_;

  base::Time last_clear_time_;

  // Clock for getting time.
  std::unique_ptr<base::Clock> clock_;

  base::WeakPtrFactory<OfflinePageStorageManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageStorageManager);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_STORAGE_MANAGER_H_
