// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_TEST_UTIL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_TEST_UTIL_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"

namespace base {
class ScopedTempDir;
class SimpleTestClock;
}  // namespace base

namespace offline_pages {

// Encapsulates the OfflinePageMetadataStoreSQL and provides synchronous
// operations on the store, for test writing convenience.
class OfflinePageMetadataStoreTestUtil {
 public:
  explicit OfflinePageMetadataStoreTestUtil(
      scoped_refptr<base::TestSimpleTaskRunner> task_runner);
  ~OfflinePageMetadataStoreTestUtil();

  // Builds a new store in a temporary directory.
  void BuildStore();
  // Builds the store in memory (no disk storage).
  void BuildStoreInMemory();
  // Releases the ownership of currently controlled store.
  std::unique_ptr<OfflinePageMetadataStoreSQL> ReleaseStore();
  // Deletes the currently held store that was previously built.
  void DeleteStore();

  // Inserts an offline page item into the store.
  void InsertItem(const OfflinePageItem& item);

  OfflinePageMetadataStoreSQL* store() { return store_.get(); }

  base::SimpleTestClock* clock() { return &clock_; }

 private:
  void RunUntilIdle();

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<OfflinePageMetadataStoreSQL> store_;
  base::SimpleTestClock clock_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMetadataStoreTestUtil);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_TEST_UTIL_H_
