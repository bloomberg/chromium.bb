// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_metadata_change_list.h"

#include "base/location.h"

using syncer::SyncError;

namespace autofill {

AutofillMetadataChangeList::AutofillMetadataChangeList(AutofillTable* table,
                                                       syncer::ModelType type)
    : table_(table), type_(type) {
  DCHECK(table_);
  // This should be changed as new autofill types are converted to USS.
  DCHECK_EQ(syncer::AUTOFILL, type_);
}

AutofillMetadataChangeList::~AutofillMetadataChangeList() {
  DCHECK(!error_.IsSet());
}

void AutofillMetadataChangeList::UpdateModelTypeState(
    const sync_pb::ModelTypeState& model_type_state) {
  if (error_.IsSet()) {
    return;
  }

  if (!table_->UpdateModelTypeState(type_, model_type_state)) {
    error_ = SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                       "Failed to update ModelTypeState.", type_);
  }
}

void AutofillMetadataChangeList::ClearModelTypeState() {
  if (error_.IsSet()) {
    return;
  }

  if (!table_->ClearModelTypeState(type_)) {
    error_ = SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                       "Failed to clear ModelTypeState.", type_);
  }
}

void AutofillMetadataChangeList::UpdateMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  if (error_.IsSet()) {
    return;
  }

  if (!table_->UpdateSyncMetadata(type_, storage_key, metadata)) {
    error_ = SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                       "Failed to update entity metadata.", type_);
  }
}

void AutofillMetadataChangeList::ClearMetadata(const std::string& storage_key) {
  if (error_.IsSet()) {
    return;
  }

  if (!table_->ClearSyncMetadata(type_, storage_key)) {
    error_ = SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                       "Failed to clear entity metadata.", type_);
  }
}

SyncError AutofillMetadataChangeList::TakeError() {
  SyncError e = error_;
  error_ = SyncError();
  return e;
}

}  // namespace autofill
