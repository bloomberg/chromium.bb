// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/model_task_test_base.h"

#include "base/files/file_util.h"
#include "base/logging.h"

namespace offline_pages {
ModelTaskTestBase::ModelTaskTestBase() : store_test_util_(task_runner()) {}
ModelTaskTestBase::~ModelTaskTestBase() {}

void ModelTaskTestBase::SetUp() {
  TaskTestBase::SetUp();
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  generator()->SetArchiveDirectory(TemporaryDir());
  store_test_util_.BuildStoreInMemory();
}

void ModelTaskTestBase::TearDown() {
  store_test_util_.DeleteStore();
  TaskTestBase::TearDown();
}

OfflinePageItem ModelTaskTestBase::AddPage() {
  OfflinePageItem page = generator_.CreateItemWithTempFile();
  store_test_util_.InsertItem(page);
  return page;
}

OfflinePageItem ModelTaskTestBase::AddPageWithoutFile() {
  OfflinePageItem page = generator_.CreateItemWithTempFile();
  EXPECT_TRUE(base::DeleteFile(page.file_path, false));
  store_test_util_.InsertItem(page);
  return page;
}

const base::FilePath& ModelTaskTestBase::TemporaryDir() {
  return temporary_dir_.GetPath();
}

}  // namespace offline_pages
