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

// static
std::unique_ptr<ModelTypeStore>
ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(ModelType type) {
  base::RunLoop loop;
  std::unique_ptr<ModelTypeStore> store;

  ModelTypeStore::CreateInMemoryStoreForTest(
      type,
      base::BindOnce(
          [](base::RunLoop* loop, std::unique_ptr<ModelTypeStore>* out_store,
             Result result, std::unique_ptr<ModelTypeStore> in_store) {
            ASSERT_EQ(Result::SUCCESS, result);
            *out_store = std::move(in_store);
            loop->Quit();
          },
          &loop, &store));

  // Force the initialization to run now, synchronously.
  loop.Run();

  EXPECT_TRUE(store);
  return store;
}

// static
RepeatingModelTypeStoreFactory
ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest() {
  return base::BindRepeating([](ModelType type,
                                ModelTypeStore::InitCallback callback) {
    std::move(callback).Run(Result::SUCCESS, CreateInMemoryStoreForTest(type));
  });
}

// static
void ModelTypeStoreTestUtil::MoveStoreToCallback(
    std::unique_ptr<ModelTypeStore> store,
    ModelType type,
    ModelTypeStore::InitCallback callback) {
  ASSERT_TRUE(store);
  std::move(callback).Run(Result::SUCCESS, std::move(store));
}

}  // namespace syncer
