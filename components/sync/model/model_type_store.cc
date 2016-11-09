// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_store.h"

#include "base/memory/ptr_util.h"
#include "components/sync/model_impl/accumulating_metadata_change_list.h"
#include "components/sync/model_impl/model_type_store_impl.h"
#include "components/sync/model_impl/passthrough_metadata_change_list.h"

namespace syncer {

// static
void ModelTypeStore::CreateInMemoryStoreForTest(ModelType type,
                                                const InitCallback& callback) {
  ModelTypeStoreImpl::CreateInMemoryStoreForTest(type, callback);
}

// static
void ModelTypeStore::CreateStore(
    ModelType type,
    const std::string& path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const InitCallback& callback) {
  ModelTypeStoreImpl::CreateStore(type, path, blocking_task_runner, callback);
}

ModelTypeStore::~ModelTypeStore() {}

// static
std::unique_ptr<MetadataChangeList>
ModelTypeStore::WriteBatch::CreateMetadataChangeList() {
  return base::MakeUnique<AccumulatingMetadataChangeList>();
}

ModelTypeStore::WriteBatch::WriteBatch(ModelTypeStore* store) : store_(store) {}

ModelTypeStore::WriteBatch::~WriteBatch() {}

void ModelTypeStore::WriteBatch::WriteData(const std::string& id,
                                           const std::string& value) {
  store_->WriteData(this, id, value);
}

void ModelTypeStore::WriteBatch::DeleteData(const std::string& id) {
  store_->DeleteData(this, id);
}

MetadataChangeList* ModelTypeStore::WriteBatch::GetMetadataChangeList() {
  if (!metadata_change_list_) {
    metadata_change_list_ =
        base::MakeUnique<PassthroughMetadataChangeList>(store_, this);
  }
  return metadata_change_list_.get();
}

void ModelTypeStore::WriteBatch::TransferMetadataChanges(
    std::unique_ptr<MetadataChangeList> mcl) {
  static_cast<AccumulatingMetadataChangeList*>(mcl.get())->TransferChanges(
      store_, this);
}

}  // namespace syncer
