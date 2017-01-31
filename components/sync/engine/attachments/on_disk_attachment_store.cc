// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/attachments/on_disk_attachment_store.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/engine/attachments/attachment_util.h"
#include "components/sync/engine_impl/attachments/proto/attachment_store.pb.h"
#include "components/sync/protocol/attachments.pb.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace syncer {

namespace {

// Prefix for records containing attachment data.
const char kDataPrefix[] = "data-";

// Prefix for records containing attachment metadata.
const char kMetadataPrefix[] = "metadata-";

const char kDatabaseMetadataKey[] = "database-metadata";

const int32_t kCurrentSchemaVersion = 1;

const base::FilePath::CharType kLeveldbDirectory[] =
    FILE_PATH_LITERAL("leveldb");

// Converts AttachmentStore::Component values into
// attachment_store_pb::RecordMetadata::Component.
attachment_store_pb::RecordMetadata::Component ComponentToProto(
    AttachmentStore::Component component) {
  switch (component) {
    case AttachmentStore::MODEL_TYPE:
      return attachment_store_pb::RecordMetadata::MODEL_TYPE;
    case AttachmentStore::SYNC:
      return attachment_store_pb::RecordMetadata::SYNC;
  }
  NOTREACHED();
  return attachment_store_pb::RecordMetadata::UNKNOWN;
}

leveldb::WriteOptions MakeWriteOptions() {
  leveldb::WriteOptions write_options;
  write_options.sync = true;
  return write_options;
}

leveldb::ReadOptions MakeNonCachingReadOptions() {
  leveldb::ReadOptions read_options;
  read_options.fill_cache = false;
  read_options.verify_checksums = true;
  return read_options;
}

leveldb::ReadOptions MakeCachingReadOptions() {
  leveldb::ReadOptions read_options;
  read_options.fill_cache = true;
  read_options.verify_checksums = true;
  return read_options;
}

leveldb::Status ReadStoreMetadata(
    leveldb::DB* db,
    attachment_store_pb::StoreMetadata* metadata) {
  std::string data_str;

  leveldb::Status status =
      db->Get(MakeCachingReadOptions(), kDatabaseMetadataKey, &data_str);
  if (!status.ok())
    return status;
  if (!metadata->ParseFromString(data_str))
    return leveldb::Status::Corruption("Metadata record corruption");
  return leveldb::Status::OK();
}

leveldb::Status WriteStoreMetadata(
    leveldb::DB* db,
    const attachment_store_pb::StoreMetadata& metadata) {
  std::string data_str;

  metadata.SerializeToString(&data_str);
  return db->Put(MakeWriteOptions(), kDatabaseMetadataKey, data_str);
}

// Adds reference to component into RecordMetadata::component set.
// Returns true if record_metadata was modified and needs to be written to disk.
bool SetReferenceInRecordMetadata(
    attachment_store_pb::RecordMetadata* record_metadata,
    attachment_store_pb::RecordMetadata::Component proto_component) {
  DCHECK(record_metadata);
  for (const int component : record_metadata->component()) {
    if (component == proto_component)
      return false;
  }
  record_metadata->add_component(proto_component);
  return true;
}

// Drops reference to component from RecordMetadata::component set.
// Returns true if record_metadata was modified and needs to be written to disk.
bool DropReferenceInRecordMetadata(
    attachment_store_pb::RecordMetadata* record_metadata,
    attachment_store_pb::RecordMetadata::Component proto_component) {
  DCHECK(record_metadata);
  bool component_removed = false;
  ::google::protobuf::RepeatedField<int>* mutable_components =
      record_metadata->mutable_component();
  for (int i = 0; i < mutable_components->size();) {
    if (mutable_components->Get(i) == proto_component) {
      if (i < mutable_components->size() - 1) {
        // Don't swap last element with itself.
        mutable_components->SwapElements(i, mutable_components->size() - 1);
      }
      mutable_components->RemoveLast();
      component_removed = true;
    } else {
      ++i;
    }
  }
  return component_removed;
}

bool AttachmentHasReferenceFromComponent(
    const attachment_store_pb::RecordMetadata& record_metadata,
    attachment_store_pb::RecordMetadata::Component proto_component) {
  for (const auto& reference_component : record_metadata.component()) {
    if (reference_component == proto_component) {
      return true;
    }
  }
  return false;
}

}  // namespace

OnDiskAttachmentStore::OnDiskAttachmentStore(
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner,
    const base::FilePath& path)
    : AttachmentStoreBackend(callback_task_runner), path_(path) {}

OnDiskAttachmentStore::~OnDiskAttachmentStore() {}

void OnDiskAttachmentStore::Init(
    const AttachmentStore::InitCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code = OpenOrCreate(path_);
  UMA_HISTOGRAM_ENUMERATION("Sync.Attachments.StoreInitResult", result_code,
                            AttachmentStore::RESULT_SIZE);
  PostCallback(base::Bind(callback, result_code));
}

void OnDiskAttachmentStore::Read(
    AttachmentStore::Component component,
    const AttachmentIdList& ids,
    const AttachmentStore::ReadCallback& callback) {
  DCHECK(CalledOnValidThread());
  std::unique_ptr<AttachmentMap> result_map(new AttachmentMap());
  std::unique_ptr<AttachmentIdList> unavailable_attachments(
      new AttachmentIdList());

  AttachmentStore::Result result_code =
      AttachmentStore::STORE_INITIALIZATION_FAILED;

  if (db_) {
    result_code = AttachmentStore::SUCCESS;
    for (const auto& id : ids) {
      std::unique_ptr<Attachment> attachment;
      attachment = ReadSingleAttachment(id, component);
      if (attachment) {
        result_map->insert(std::make_pair(id, *attachment));
      } else {
        unavailable_attachments->push_back(id);
      }
    }
    result_code = unavailable_attachments->empty()
                      ? AttachmentStore::SUCCESS
                      : AttachmentStore::UNSPECIFIED_ERROR;
  } else {
    *unavailable_attachments = ids;
  }

  PostCallback(base::Bind(callback, result_code, base::Passed(&result_map),
                          base::Passed(&unavailable_attachments)));
}

void OnDiskAttachmentStore::Write(
    AttachmentStore::Component component,
    const AttachmentList& attachments,
    const AttachmentStore::WriteCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code =
      AttachmentStore::STORE_INITIALIZATION_FAILED;

  if (db_) {
    result_code = AttachmentStore::SUCCESS;
    AttachmentList::const_iterator iter = attachments.begin();
    const AttachmentList::const_iterator end = attachments.end();
    for (; iter != end; ++iter) {
      if (!WriteSingleAttachment(*iter, component))
        result_code = AttachmentStore::UNSPECIFIED_ERROR;
    }
  }
  PostCallback(base::Bind(callback, result_code));
}

void OnDiskAttachmentStore::SetReference(AttachmentStore::Component component,
                                         const AttachmentIdList& ids) {
  DCHECK(CalledOnValidThread());
  if (!db_)
    return;
  attachment_store_pb::RecordMetadata::Component proto_component =
      ComponentToProto(component);
  for (const auto& id : ids) {
    attachment_store_pb::RecordMetadata record_metadata;
    if (!ReadSingleRecordMetadata(id, &record_metadata))
      continue;
    if (SetReferenceInRecordMetadata(&record_metadata, proto_component))
      WriteSingleRecordMetadata(id, record_metadata);
  }
}

void OnDiskAttachmentStore::DropReference(
    AttachmentStore::Component component,
    const AttachmentIdList& ids,
    const AttachmentStore::DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code =
      AttachmentStore::STORE_INITIALIZATION_FAILED;
  if (db_) {
    attachment_store_pb::RecordMetadata::Component proto_component =
        ComponentToProto(component);
    result_code = AttachmentStore::SUCCESS;
    leveldb::WriteOptions write_options = MakeWriteOptions();
    for (const auto& id : ids) {
      attachment_store_pb::RecordMetadata record_metadata;
      if (!ReadSingleRecordMetadata(id, &record_metadata))
        continue;  // Record not found.
      if (!DropReferenceInRecordMetadata(&record_metadata, proto_component))
        continue;  // Component is not in components set. Metadata was not
                   // updated.
      if (record_metadata.component_size() == 0) {
        // Last reference dropped. Need to delete attachment.
        leveldb::WriteBatch write_batch;
        write_batch.Delete(MakeDataKeyFromAttachmentId(id));
        write_batch.Delete(MakeMetadataKeyFromAttachmentId(id));

        leveldb::Status status = db_->Write(write_options, &write_batch);
        if (!status.ok()) {
          // DB::Delete doesn't check if record exists, it returns ok just like
          // AttachmentStore::Drop should.
          DVLOG(1) << "DB::Write failed: status=" << status.ToString();
          result_code = AttachmentStore::UNSPECIFIED_ERROR;
        }
      } else {
        WriteSingleRecordMetadata(id, record_metadata);
      }
    }
  }
  PostCallback(base::Bind(callback, result_code));
}

void OnDiskAttachmentStore::ReadMetadataById(
    AttachmentStore::Component component,
    const AttachmentIdList& ids,
    const AttachmentStore::ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code =
      AttachmentStore::STORE_INITIALIZATION_FAILED;
  std::unique_ptr<AttachmentMetadataList> metadata_list(
      new AttachmentMetadataList());
  if (db_) {
    result_code = AttachmentStore::SUCCESS;
    for (const auto& id : ids) {
      attachment_store_pb::RecordMetadata record_metadata;
      if (!ReadSingleRecordMetadata(id, &record_metadata)) {
        result_code = AttachmentStore::UNSPECIFIED_ERROR;
        continue;
      }
      if (!AttachmentHasReferenceFromComponent(record_metadata,
                                               ComponentToProto(component))) {
        result_code = AttachmentStore::UNSPECIFIED_ERROR;
        continue;
      }
      metadata_list->push_back(MakeAttachmentMetadata(id, record_metadata));
    }
  }
  PostCallback(base::Bind(callback, result_code, base::Passed(&metadata_list)));
}

void OnDiskAttachmentStore::ReadMetadata(
    AttachmentStore::Component component,
    const AttachmentStore::ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code =
      AttachmentStore::STORE_INITIALIZATION_FAILED;
  std::unique_ptr<AttachmentMetadataList> metadata_list(
      new AttachmentMetadataList());

  if (db_) {
    attachment_store_pb::RecordMetadata::Component proto_component =
        ComponentToProto(component);
    result_code = AttachmentStore::SUCCESS;
    std::unique_ptr<leveldb::Iterator> db_iterator(
        db_->NewIterator(MakeNonCachingReadOptions()));
    DCHECK(db_iterator);
    for (db_iterator->Seek(kMetadataPrefix); db_iterator->Valid();
         db_iterator->Next()) {
      leveldb::Slice key = db_iterator->key();
      if (!key.starts_with(kMetadataPrefix)) {
        break;
      }
      // Make AttachmentId from levelDB key.
      key.remove_prefix(strlen(kMetadataPrefix));
      sync_pb::AttachmentIdProto id_proto;
      id_proto.set_unique_id(key.ToString());
      AttachmentId id = AttachmentId::CreateFromProto(id_proto);
      // Parse metadata record.
      attachment_store_pb::RecordMetadata record_metadata;
      if (!record_metadata.ParseFromString(db_iterator->value().ToString())) {
        DVLOG(1) << "RecordMetadata::ParseFromString failed";
        result_code = AttachmentStore::UNSPECIFIED_ERROR;
        continue;
      }
      DCHECK_GT(record_metadata.component_size(), 0);
      if (AttachmentHasReferenceFromComponent(record_metadata, proto_component))
        metadata_list->push_back(MakeAttachmentMetadata(id, record_metadata));
    }

    if (!db_iterator->status().ok()) {
      DVLOG(1) << "DB Iterator failed: status="
               << db_iterator->status().ToString();
      result_code = AttachmentStore::UNSPECIFIED_ERROR;
    }
  }

  PostCallback(base::Bind(callback, result_code, base::Passed(&metadata_list)));
}

AttachmentStore::Result OnDiskAttachmentStore::OpenOrCreate(
    const base::FilePath& path) {
  DCHECK(!db_);
  base::FilePath leveldb_path = path.Append(kLeveldbDirectory);

  leveldb::DB* db_raw;
  std::unique_ptr<leveldb::DB> db;
  leveldb::Options options;
  options.create_if_missing = true;
  options.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;
  // TODO(pavely): crbug/424287 Consider adding info_log, block_cache and
  // filter_policy to options.
  leveldb::Status status =
      leveldb::DB::Open(options, leveldb_path.AsUTF8Unsafe(), &db_raw);
  if (!status.ok()) {
    DVLOG(1) << "DB::Open failed: status=" << status.ToString()
             << ", path=" << path.AsUTF8Unsafe();
    return AttachmentStore::UNSPECIFIED_ERROR;
  }

  db.reset(db_raw);

  attachment_store_pb::StoreMetadata metadata;
  status = ReadStoreMetadata(db.get(), &metadata);
  if (!status.ok() && !status.IsNotFound()) {
    DVLOG(1) << "ReadStoreMetadata failed: status=" << status.ToString();
    return AttachmentStore::UNSPECIFIED_ERROR;
  }
  if (status.IsNotFound()) {
    // Brand new database.
    metadata.set_schema_version(kCurrentSchemaVersion);
    status = WriteStoreMetadata(db.get(), metadata);
    if (!status.ok()) {
      DVLOG(1) << "WriteStoreMetadata failed: status=" << status.ToString();
      return AttachmentStore::UNSPECIFIED_ERROR;
    }
  }
  DCHECK(status.ok());

  // Upgrade code goes here.

  if (metadata.schema_version() != kCurrentSchemaVersion) {
    DVLOG(1) << "Unknown schema version: " << metadata.schema_version();
    return AttachmentStore::UNSPECIFIED_ERROR;
  }

  db_ = std::move(db);
  return AttachmentStore::SUCCESS;
}

std::unique_ptr<Attachment> OnDiskAttachmentStore::ReadSingleAttachment(
    const AttachmentId& attachment_id,
    AttachmentStore::Component component) {
  std::unique_ptr<Attachment> attachment;
  attachment_store_pb::RecordMetadata record_metadata;
  if (!ReadSingleRecordMetadata(attachment_id, &record_metadata)) {
    return attachment;
  }
  if (!AttachmentHasReferenceFromComponent(record_metadata,
                                           ComponentToProto(component)))
    return attachment;

  const std::string key = MakeDataKeyFromAttachmentId(attachment_id);
  std::string data_str;
  leveldb::Status status =
      db_->Get(MakeNonCachingReadOptions(), key, &data_str);
  if (!status.ok()) {
    DVLOG(1) << "DB::Get for data failed: status=" << status.ToString();
    return attachment;
  }
  scoped_refptr<base::RefCountedMemory> data =
      base::RefCountedString::TakeString(&data_str);
  uint32_t crc32c = ComputeCrc32c(data);
  if (record_metadata.has_crc32c()) {
    if (record_metadata.crc32c() != crc32c) {
      DVLOG(1) << "Attachment crc32c does not match value read from store";
      return attachment;
    }
    if (record_metadata.crc32c() != attachment_id.GetCrc32c()) {
      DVLOG(1) << "Attachment crc32c does not match value in AttachmentId";
      return attachment;
    }
  }
  attachment = base::MakeUnique<Attachment>(
      Attachment::CreateFromParts(attachment_id, data));
  return attachment;
}

bool OnDiskAttachmentStore::WriteSingleAttachment(
    const Attachment& attachment,
    AttachmentStore::Component component) {
  const std::string metadata_key =
      MakeMetadataKeyFromAttachmentId(attachment.GetId());
  const std::string data_key = MakeDataKeyFromAttachmentId(attachment.GetId());

  std::string metadata_str;
  leveldb::Status status =
      db_->Get(MakeCachingReadOptions(), metadata_key, &metadata_str);
  if (status.ok()) {
    // Entry exists, don't overwrite.
    return true;
  } else if (!status.IsNotFound()) {
    // Entry exists but failed to read.
    DVLOG(1) << "DB::Get failed: status=" << status.ToString();
    return false;
  }
  DCHECK(status.IsNotFound());

  leveldb::WriteBatch write_batch;
  // Write metadata.
  attachment_store_pb::RecordMetadata metadata;
  metadata.set_attachment_size(attachment.GetData()->size());
  metadata.set_crc32c(attachment.GetCrc32c());
  SetReferenceInRecordMetadata(&metadata, ComponentToProto(component));
  metadata_str = metadata.SerializeAsString();
  write_batch.Put(metadata_key, metadata_str);
  // Write data.
  scoped_refptr<base::RefCountedMemory> data = attachment.GetData();
  leveldb::Slice data_slice(data->front_as<char>(), data->size());
  write_batch.Put(data_key, data_slice);

  status = db_->Write(MakeWriteOptions(), &write_batch);
  if (!status.ok()) {
    // Failed to write.
    DVLOG(1) << "DB::Write failed: status=" << status.ToString();
    return false;
  }
  return true;
}

bool OnDiskAttachmentStore::ReadSingleRecordMetadata(
    const AttachmentId& attachment_id,
    attachment_store_pb::RecordMetadata* record_metadata) {
  DCHECK(record_metadata);
  const std::string metadata_key =
      MakeMetadataKeyFromAttachmentId(attachment_id);
  std::string metadata_str;
  leveldb::Status status =
      db_->Get(MakeCachingReadOptions(), metadata_key, &metadata_str);
  if (!status.ok()) {
    DVLOG(1) << "DB::Get for metadata failed: status=" << status.ToString();
    return false;
  }
  if (!record_metadata->ParseFromString(metadata_str)) {
    DVLOG(1) << "RecordMetadata::ParseFromString failed";
    return false;
  }
  DCHECK_GT(record_metadata->component_size(), 0);
  return true;
}

bool OnDiskAttachmentStore::WriteSingleRecordMetadata(
    const AttachmentId& attachment_id,
    const attachment_store_pb::RecordMetadata& record_metadata) {
  const std::string metadata_key =
      MakeMetadataKeyFromAttachmentId(attachment_id);
  std::string metadata_str;
  metadata_str = record_metadata.SerializeAsString();
  leveldb::Status status =
      db_->Put(MakeWriteOptions(), metadata_key, metadata_str);
  if (!status.ok()) {
    DVLOG(1) << "DB::Put failed: status=" << status.ToString();
    return false;
  }
  return true;
}

std::string OnDiskAttachmentStore::MakeDataKeyFromAttachmentId(
    const AttachmentId& attachment_id) {
  std::string key = kDataPrefix + attachment_id.GetProto().unique_id();
  return key;
}

std::string OnDiskAttachmentStore::MakeMetadataKeyFromAttachmentId(
    const AttachmentId& attachment_id) {
  std::string key = kMetadataPrefix + attachment_id.GetProto().unique_id();
  return key;
}

AttachmentMetadata OnDiskAttachmentStore::MakeAttachmentMetadata(
    const AttachmentId& attachment_id,
    const attachment_store_pb::RecordMetadata& record_metadata) {
  return AttachmentMetadata(attachment_id, record_metadata.attachment_size());
}

}  // namespace syncer
