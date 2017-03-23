// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNC_METADATA_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNC_METADATA_DATABASE_H_

#include "base/macros.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "sql/meta_table.h"

namespace sql {
class Connection;
}

namespace history {

// A sync metadata database needs to maintain two tables: entity metadata table
// and datatype state table. Entity metadata table contains metadata(sync
// states) for each url. Datatype state table contains the state of typed url
// datatype.
class TypedURLSyncMetadataDatabase {
 public:
  // Must call InitVisitTable() before using to make sure the database is
  // initialized.
  TypedURLSyncMetadataDatabase();
  virtual ~TypedURLSyncMetadataDatabase();

  // Read all the stored metadata for typed URL and fill |metadata_batch|
  // with it.
  bool GetAllSyncMetadata(syncer::MetadataBatch* metadata_batch);

  // Update the metadata row for typed URL, keyed by |storage_key|, to
  // contain the contents of |metadata|.
  bool UpdateSyncMetadata(const std::string& storage_key,
                          const sync_pb::EntityMetadata& metadata);

  // Remove the metadata row of typed URL keyed by |storage_key|.
  bool ClearSyncMetadata(const std::string& storage_key);

  // Update the stored sync state for the typed URL.
  bool UpdateModelTypeState(const sync_pb::ModelTypeState& model_type_state);

  // Clear the stored sync state for typed URL.
  bool ClearModelTypeState();

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Connection& GetDB() = 0;

  // Returns MetaTable, so this sync can store ModelTypeState in MetaTable.
  // Check if GetMetaTable().GetVersionNumber() is greater than 0 to make sure
  // MetaTable is initialed.
  virtual sql::MetaTable& GetMetaTable() = 0;

  // Called by the derived classes on initialization to make sure the tables
  // and indices are properly set up. Must be called before anything else.
  bool InitSyncTable();

 private:
  // Read all sync_pb::EntityMetadata for typed URL and fill
  // |metadata_records| with it.
  bool GetAllSyncEntityMetadata(syncer::MetadataBatch* metadata_batch);

  // Read sync_pb::ModelTypeState for typed URL and fill |state| with it.
  bool GetModelTypeState(sync_pb::ModelTypeState* state);

  DISALLOW_COPY_AND_ASSIGN(TypedURLSyncMetadataDatabase);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNC_METADATA_DATABASE_H_
