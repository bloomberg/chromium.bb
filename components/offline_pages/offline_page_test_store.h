// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_TEST_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_TEST_STORE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_metadata_store.h"

namespace offline_pages {

// Offline page store to be used for testing purposes. It stores offline pages
// in memory. All callbacks are posted immediately through a provided
// |task_runner|.
class OfflinePageTestStore : public OfflinePageMetadataStore {
 public:
  enum class TestScenario {
    SUCCESSFUL,
    WRITE_FAILED,
    LOAD_FAILED,
    REMOVE_FAILED,
  };

  explicit OfflinePageTestStore(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  explicit OfflinePageTestStore(const OfflinePageTestStore& other_store);
  ~OfflinePageTestStore() override;

  // OfflinePageMetadataStore overrides:
  void Load(const LoadCallback& callback) override;
  void AddOrUpdateOfflinePage(const OfflinePageItem& offline_page,
                              const UpdateCallback& callback) override;
  void RemoveOfflinePages(const std::vector<int64>& bookmark_ids,
                          const UpdateCallback& callback) override;
  void Reset(const ResetCallback& callback) override;

  void UpdateLastAccessTime(int64 bookmark_id,
                            const base::Time& last_access_time);

  const OfflinePageItem& last_saved_page() const { return last_saved_page_; }

  void set_test_scenario(TestScenario scenario) { scenario_ = scenario; };

  const std::vector<OfflinePageItem>& offline_pages() const {
    return offline_pages_;
  }

 private:
  OfflinePageItem last_saved_page_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  TestScenario scenario_;

  std::vector<OfflinePageItem> offline_pages_;

  DISALLOW_ASSIGN(OfflinePageTestStore);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_TEST_STORE_H_
