// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_sync_metadata_database.h"

#include "base/strings/string_number_conversions.h"
#include "sql/statement.h"

namespace history {

namespace {

// Key in sql::MetaTable, the value will be Serialization of sync
// ModelTypeState, which is for tracking sync state of typed url datatype.
const char kTypedURLModelTypeStateKey[] = "typed_url_model_type_state";

}  // namespace

// Description of database table:
//
// typed_url_sync_metadata
//   storage_key      the rowid of an entry in urls table, used by service to
//                    look up native data with sync metadata records.
//   value            Serialize sync EntityMetadata, which is for tracking sync
//                    state of each typed url.

TypedURLSyncMetadataDatabase::TypedURLSyncMetadataDatabase() {}

TypedURLSyncMetadataDatabase::~TypedURLSyncMetadataDatabase() {}

bool TypedURLSyncMetadataDatabase::GetAllSyncMetadata(
    syncer::MetadataBatch* metadata_batch) {
  syncer::EntityMetadataMap metadata_records_map;

  // TODO(gangwu): crbug.com/701670 remove metadata_records_map to reduce memory
  // usage and time.
  if (GetAllSyncEntityMetadata(&metadata_records_map)) {
    for (const auto& storage_key_to_metadata : metadata_records_map) {
      metadata_batch->AddMetadata(storage_key_to_metadata.first,
                                  storage_key_to_metadata.second);
    }
  } else {
    return false;
  }

  sync_pb::ModelTypeState model_type_state;
  if (GetModelTypeState(&model_type_state)) {
    metadata_batch->SetModelTypeState(model_type_state);
  } else {
    return false;
  }

  return true;
}

bool TypedURLSyncMetadataDatabase::UpdateSyncMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  int64_t storage_key_int = 0;
  if (!base::StringToInt64(storage_key, &storage_key_int)) {
    return false;
  }
  sql::Statement s(GetDB().GetUniqueStatement(
      "INSERT OR REPLACE INTO typed_url_sync_metadata "
      "(storage_key, value) VALUES(?, ?)"));
  s.BindInt64(0, storage_key_int);
  s.BindString(1, metadata.SerializeAsString());

  return s.Run();
}

bool TypedURLSyncMetadataDatabase::ClearSyncMetadata(
    const std::string& storage_key) {
  int64_t storage_key_int = 0;
  if (!base::StringToInt64(storage_key, &storage_key_int)) {
    return false;
  }
  sql::Statement s(GetDB().GetUniqueStatement(
      "DELETE FROM typed_url_sync_metadata WHERE storage_key=?"));
  s.BindInt64(0, storage_key_int);

  return s.Run();
}

bool TypedURLSyncMetadataDatabase::UpdateModelTypeState(
    const sync_pb::ModelTypeState& model_type_state) {
  DCHECK_GT(GetMetaTable().GetVersionNumber(), 0);

  std::string serialized_state = model_type_state.SerializeAsString();
  return GetMetaTable().SetValue(kTypedURLModelTypeStateKey, serialized_state);
}

bool TypedURLSyncMetadataDatabase::ClearModelTypeState() {
  DCHECK_GT(GetMetaTable().GetVersionNumber(), 0);
  return GetMetaTable().DeleteKey(kTypedURLModelTypeStateKey);
}

bool TypedURLSyncMetadataDatabase::InitSyncTable() {
  if (!GetDB().DoesTableExist("typed_url_sync_metadata")) {
    if (!GetDB().Execute("CREATE TABLE typed_url_sync_metadata ("
                         "storage_key INTEGER PRIMARY KEY NOT NULL,"
                         "value BLOB)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool TypedURLSyncMetadataDatabase::GetAllSyncEntityMetadata(
    syncer::EntityMetadataMap* metadata_records) {
  sql::Statement s(GetDB().GetUniqueStatement(
      "SELECT storage_key, value FROM typed_url_sync_metadata"));

  while (s.Step()) {
    std::string storage_key = base::Int64ToString(s.ColumnInt64(0));
    std::string serialized_metadata = s.ColumnString(1);
    sync_pb::EntityMetadata metadata_record;
    if (metadata_record.ParseFromString(serialized_metadata)) {
      metadata_records->insert(std::make_pair(storage_key, metadata_record));
    } else {
      return false;
    }
  }
  return true;
}

bool TypedURLSyncMetadataDatabase::GetModelTypeState(
    sync_pb::ModelTypeState* state) {
  DCHECK_GT(GetMetaTable().GetVersionNumber(), 0);
  std::string serialized_state;
  if (!GetMetaTable().GetValue(kTypedURLModelTypeStateKey, &serialized_state)) {
    return true;
  }

  return state->ParseFromString(serialized_state);
}

}  // namespace history
