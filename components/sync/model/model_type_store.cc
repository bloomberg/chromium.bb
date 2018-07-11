// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_store.h"

#include <utility>

#include "components/sync/model_impl/model_type_store_impl.h"

namespace syncer {

// static
void ModelTypeStore::CreateInMemoryStoreForTest(ModelType type,
                                                InitCallback callback) {
  ModelTypeStoreImpl::CreateInMemoryStoreForTest(type, std::move(callback));
}

// static
void ModelTypeStore::CreateStore(const std::string& path,
                                 ModelType type,
                                 InitCallback callback) {
  ModelTypeStoreImpl::CreateStore(type, path, std::move(callback));
}

}  // namespace syncer
