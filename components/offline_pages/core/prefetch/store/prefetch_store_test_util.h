// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_TEST_UTIL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/test_simple_task_runner.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"

class GURL;

namespace base {
class ScopedTempDir;
}  // namespace base

namespace offline_pages {
struct PrefetchItem;

// Encapsulates the PrefetchStore and provides synchronous operations on the
// store, for test writing convenience.
class PrefetchStoreTestUtil {
 public:
  static const int kStoreCommandFailed = -1;

  explicit PrefetchStoreTestUtil(
      scoped_refptr<base::TestSimpleTaskRunner> task_runner);
  ~PrefetchStoreTestUtil();

  void BuildStore();
  void BuildStoreInMemory();
  // Releases the ownership of currently controlled store.
  std::unique_ptr<PrefetchStore> ReleaseStore();
  void DeleteStore();

  // Returns auto-assigned offline_id or kStoreCommandFailed.
  int64_t InsertPrefetchItem(const PrefetchItem& item);

  // Returns the total count of prefetch items in the database.
  int CountPrefetchItems();

  // Returns nullptr if the item with specified id is not found.
  std::unique_ptr<PrefetchItem> GetPrefetchItem(int64_t offline_id);

  // Returns number of affected items.
  int ZombifyPrefetchItems(const std::string& name_space, const GURL& url);

  PrefetchStore* store() { return store_.get(); }

 private:
  void RunUntilIdle();

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<PrefetchStore> store_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchStoreTestUtil);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_STORE_TEST_UTIL_H_
