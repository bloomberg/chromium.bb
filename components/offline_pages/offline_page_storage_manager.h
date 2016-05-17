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
#include "components/offline_pages/offline_page_types.h"

namespace base {
class Clock;
}

namespace offline_pages {

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

  // Callback used when calling ClearPagesIfNeeded.
  // int: the number of deleted pages.
  // DeletePageResult: result of deleting pages.
  typedef base::Callback<void(int, DeletePageResult)> ClearPageCallback;

  explicit OfflinePageStorageManager(Client* client,
                                     ClientPolicyController* policy_controller);

  ~OfflinePageStorageManager();

  // The manager would *try* to clear pages when called. It may not delete any
  // pages (if clearing condition wasn't satisfied).
  void ClearPagesIfNeeded(const ClearPageCallback& callback);

  // Sets the clock for testing.
  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

 private:
  // Selects and removes pages that need to be expired. Triggered as a callback
  // to |GetAllPages|.
  void ClearExpiredPages(const ClearPageCallback& callback,
                         const MultipleOfflinePageItemResult& pages);

  // Gets offline IDs of all expired pages and return in |offline_ids|.
  void GetExpiredPageIds(const MultipleOfflinePageItemResult& pages,
                         std::vector<int64_t>& offline_ids);

  // Callback when expired pages has been deleted.
  void OnExpiredPagesDeleted(const ClearPageCallback& callback,
                             int pages_to_clear,
                             DeletePageResult result);

  // Determine if manager should clear pages.
  bool ShouldClearPages();

  // Return true if |page| is expired comparing to |now|.
  bool ShouldBeExpired(const base::Time& now, const OfflinePageItem& page);

  // Not owned.
  Client* client_;

  // Not owned.
  ClientPolicyController* policy_controller_;

  bool in_progress_;

  // Clock for getting time.
  std::unique_ptr<base::Clock> clock_;

  base::WeakPtrFactory<OfflinePageStorageManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageStorageManager);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_STORAGE_MANAGER_H_
