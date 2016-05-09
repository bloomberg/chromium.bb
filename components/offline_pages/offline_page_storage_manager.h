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
#include "components/offline_pages/offline_page_model.h"

namespace offline_pages {

// TODO(romax): Keep this as a default value until I find a way to get
// storage size in C++. (20MB)
static const int64_t kDefaultStorageSize = 20 * (1 << 20);

class ClientPolicyController;
struct OfflinePageItem;

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
  // Callback used when calling ClearPagesIfNeeded.
  // int: the number of deleted pages.
  // DeletePageResult: result of deleting pages.
  typedef base::Callback<void(int, OfflinePageModel::DeletePageResult)>
      ClearPageCallback;

  explicit OfflinePageStorageManager(OfflinePageModel* model);

  ~OfflinePageStorageManager();

  // The manager would *try* to clear pages when called. It may not delete any
  // pages (if clearing condition wasn't satisfied).
  void ClearPagesIfNeeded(const ClearPageCallback& callback);

 private:
  // Selects and removes pages that need to be expired. Triggered as a callback
  // to |GetAllPages|.
  void ClearExpiredPages(
      const ClearPageCallback& callback,
      const OfflinePageModel::MultipleOfflinePageItemResult& pages);

  // Gets offline IDs of all expired pages and return in |offline_ids|.
  void GetExpiredPageIds(
      const OfflinePageModel::MultipleOfflinePageItemResult& pages,
      std::vector<int64_t>& offline_ids);

  // Callback when expired pages has been deleted.
  void OnExpiredPagesDeleted(const ClearPageCallback& callback,
                             int pages_to_clear,
                             OfflinePageModel::DeletePageResult result);

  // Determine if manager should clear pages.
  bool ShouldClearPages();

  // Return true if |page| is expired.
  bool IsPageExpired(const OfflinePageItem& page);

  // Not owned.
  OfflinePageModel* model_;

  // Not owned.
  ClientPolicyController* policy_controller_;

  bool in_progress_;

  base::WeakPtrFactory<OfflinePageStorageManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageStorageManager);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_STORAGE_MANAGER_H_
