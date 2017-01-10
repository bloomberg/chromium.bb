// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/mock_model_type_store.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"

namespace syncer {

MockModelTypeStore::MockModelTypeStore() = default;
MockModelTypeStore::~MockModelTypeStore() = default;

void MockModelTypeStore::ReadData(const IdList& id_list,
                                  const ReadDataCallback& callback) {
  if (!read_data_handler_.is_null()) {
    read_data_handler_.Run(id_list, callback);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, Result::SUCCESS,
                              base::Passed(std::unique_ptr<RecordList>()),
                              base::Passed(std::unique_ptr<IdList>())));
  }
}

void MockModelTypeStore::ReadAllData(const ReadAllDataCallback& callback) {
  if (!read_all_data_handler_.is_null()) {
    read_all_data_handler_.Run(callback);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, Result::SUCCESS,
                              base::Passed(std::unique_ptr<RecordList>())));
  }
}

void MockModelTypeStore::ReadAllMetadata(const ReadMetadataCallback& callback) {
  if (!read_all_metadata_handler_.is_null()) {
    read_all_metadata_handler_.Run(callback);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, base::Optional<ModelError>(),
                              base::Passed(std::unique_ptr<MetadataBatch>())));
  }
}

std::unique_ptr<MockModelTypeStore::WriteBatch>
MockModelTypeStore::CreateWriteBatch() {
  return base::WrapUnique(new MockModelTypeStore::WriteBatch(this));
}

void MockModelTypeStore::CommitWriteBatch(
    std::unique_ptr<WriteBatch> write_batch,
    const CallbackWithResult& callback) {
  if (!commit_write_batch_handler_.is_null()) {
    commit_write_batch_handler_.Run(std::move(write_batch), callback);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, Result::SUCCESS));
  }
}

void MockModelTypeStore::WriteData(WriteBatch* write_batch,
                                   const std::string& id,
                                   const std::string& value) {
  if (!write_data_handler_.is_null()) {
    write_data_handler_.Run(write_batch, id, value);
  }
}

void MockModelTypeStore::WriteMetadata(WriteBatch* write_batch,
                                       const std::string& id,
                                       const std::string& value) {
  if (!write_metadata_handler_.is_null()) {
    write_metadata_handler_.Run(write_batch, id, value);
  }
}

void MockModelTypeStore::WriteGlobalMetadata(WriteBatch* write_batch,
                                             const std::string& value) {
  if (!write_global_metadata_handler_.is_null()) {
    write_global_metadata_handler_.Run(write_batch, value);
  }
}

void MockModelTypeStore::DeleteData(WriteBatch* write_batch,
                                    const std::string& id) {
  if (!delete_data_handler_.is_null()) {
    delete_data_handler_.Run(write_batch, id);
  }
}

void MockModelTypeStore::DeleteMetadata(WriteBatch* write_batch,
                                        const std::string& id) {
  if (!delete_metadata_handler_.is_null()) {
    delete_metadata_handler_.Run(write_batch, id);
  }
}

void MockModelTypeStore::DeleteGlobalMetadata(WriteBatch* write_batch) {
  if (!delete_global_metadata_handler_.is_null()) {
    delete_global_metadata_handler_.Run(write_batch);
  }
}

void MockModelTypeStore::RegisterReadDataHandler(
    const ReadDataSignature& handler) {
  read_data_handler_ = handler;
}

void MockModelTypeStore::RegisterReadAllDataHandler(
    const ReadAllDataSignature& handler) {
  read_all_data_handler_ = handler;
}

void MockModelTypeStore::RegisterReadAllMetadataHandler(
    const ReadAllMetadataSignature& handler) {
  read_all_metadata_handler_ = handler;
}

void MockModelTypeStore::RegisterCommitWriteBatchHandler(
    const CommitWriteBatchSignature& handler) {
  commit_write_batch_handler_ = handler;
}

void MockModelTypeStore::RegisterWriteDataHandler(
    const WriteRecordSignature& handler) {
  write_data_handler_ = handler;
}

void MockModelTypeStore::RegisterWriteMetadataHandler(
    const WriteRecordSignature& handler) {
  write_metadata_handler_ = handler;
}

void MockModelTypeStore::RegisterWriteGlobalMetadataHandler(
    const WriteGlobalMetadataSignature& handler) {
  write_global_metadata_handler_ = handler;
}

void MockModelTypeStore::RegisterDeleteDataHandler(
    const DeleteRecordSignature& handler) {
  delete_data_handler_ = handler;
}

void MockModelTypeStore::RegisterDeleteMetadataHandler(
    const DeleteRecordSignature& handler) {
  delete_metadata_handler_ = handler;
}

void MockModelTypeStore::RegisterDeleteGlobalMetadataHandler(
    const DeleteGlobalMetadataSignature& handler) {
  delete_global_metadata_handler_ = handler;
}

}  // namespace syncer
