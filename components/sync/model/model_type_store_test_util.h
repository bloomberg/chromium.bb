// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_TEST_UTIL_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_TEST_UTIL_H_

#include <memory>

#include "components/sync/model/model_type_store.h"

namespace syncer {

// Util class with several static methods to facilitate writing unit tests for
// classes that use ModelTypeStore objects.
class ModelTypeStoreTestUtil {
 public:
  // Creates an in memory store synchronously. Be aware that to do this all
  // outstanding tasks will be run as the current message loop is pumped.
  static std::unique_ptr<ModelTypeStore> CreateInMemoryStoreForTest();

  // Creates a factory callback to synchronously return an in memory store.
  static ModelTypeStoreFactory FactoryForInMemoryStoreForTest();

  // Can be curried with an owned store object to allow passing an already
  // created store to a service constructor in a unit test.
  static void MoveStoreToCallback(std::unique_ptr<ModelTypeStore> store,
                                  ModelType type,
                                  const ModelTypeStore::InitCallback& callback);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_TEST_UTIL_H_
