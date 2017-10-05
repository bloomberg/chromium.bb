// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/clear_digest_task.h"

#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const char kTestDigest[] = "ktesTDIgest==";
}  // namespace

class ClearDigestTaskTest : public testing::Test {
 public:
  ClearDigestTaskTest();
  ~ClearDigestTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  OfflinePageItemGenerator* generator() { return &generator_; }
  TestTaskRunner* runner() { return &runner_; }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  TestTaskRunner runner_;
};

ClearDigestTaskTest::ClearDigestTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_) {}

ClearDigestTaskTest::~ClearDigestTaskTest() {}

void ClearDigestTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
}

void ClearDigestTaskTest::TearDown() {
  store_test_util_.DeleteStore();
}

TEST_F(ClearDigestTaskTest, ClearDigest) {
  OfflinePageItem page = generator()->CreateItem();
  page.digest = kTestDigest;
  store_test_util()->InsertItem(page);

  auto task = base::MakeUnique<ClearDigestTask>(store(), page.offline_id);
  runner()->RunTask(std::move(task));

  // Check the digest of the page is cleared.
  auto offline_page = store_test_util()->GetPageByOfflineId(page.offline_id);
  EXPECT_TRUE(offline_page);
  EXPECT_TRUE(offline_page->digest.empty());
}

}  // namespace offline_pages
