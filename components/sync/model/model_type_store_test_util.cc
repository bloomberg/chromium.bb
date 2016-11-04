// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_store_test_util.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "components/sync/base/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using Result = ModelTypeStore::Result;

namespace {

void MoveStoreToScopedPtr(std::unique_ptr<ModelTypeStore>* out_store,
                          Result result,
                          std::unique_ptr<ModelTypeStore> in_store) {
  ASSERT_EQ(Result::SUCCESS, result);
  std::swap(*out_store, in_store);
}

}  // namespace

// static
std::unique_ptr<ModelTypeStore>
ModelTypeStoreTestUtil::CreateInMemoryStoreForTest() {
  std::unique_ptr<ModelTypeStore> store;
  ModelTypeStore::CreateInMemoryStoreForTest(
      UNSPECIFIED, base::Bind(&MoveStoreToScopedPtr, &store));

  // Force the initialization to run now, synchronously.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(store);
  return store;
}

// static
void ModelTypeStoreTestUtil::MoveStoreToCallback(
    std::unique_ptr<ModelTypeStore> store,
    const ModelTypeStore::InitCallback& callback) {
  ASSERT_TRUE(store);
  callback.Run(Result::SUCCESS, std::move(store));
}

}  // namespace syncer
