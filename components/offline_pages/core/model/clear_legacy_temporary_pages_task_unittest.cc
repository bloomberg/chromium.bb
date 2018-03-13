// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/clear_legacy_temporary_pages_task.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class ClearLegacyTemporaryPagesTaskTest : public ModelTaskTestBase {
 public:
  ~ClearLegacyTemporaryPagesTaskTest() override {}

  OfflinePageItem AddPage(const std::string& name_space) {
    generator()->SetNamespace(name_space);
    OfflinePageItem page = generator()->CreateItemWithTempFile();
    store_test_util()->InsertItem(page);
    return page;
  }
};

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeletePageInLegacyArchivesDir \
  DISABLED_TestDeletePageInLegacyArchivesDir
#else
#define MAYBE_TestDeletePageInLegacyArchivesDir \
  TestDeletePageInLegacyArchivesDir
#endif
TEST_F(ClearLegacyTemporaryPagesTaskTest,
       MAYBE_TestDeletePageInLegacyArchivesDir) {
  OfflinePageItem page1 = AddPage(kLastNNamespace);
  OfflinePageItem page2 = AddPage(kDownloadNamespace);

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_utils::GetFileCountInDirectory(TemporaryDir()));

  auto task = std::make_unique<ClearLegacyTemporaryPagesTask>(
      store(), policy_controller(), TemporaryDir());
  RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_utils::GetFileCountInDirectory(TemporaryDir()));
  EXPECT_FALSE(store_test_util()->GetPageByOfflineId(page1.offline_id));
}

}  // namespace offline_pages
