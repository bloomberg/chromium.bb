// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_metadata_change_list.h"

#include "base/location.h"

using base::Optional;
using syncer::ModelError;

namespace autofill {

AutofillMetadataChangeList::AutofillMetadataChangeList(AutofillTable* table,
                                                       syncer::ModelType type)
    : table_(table), type_(type) {
  DCHECK(table_);
  // This should be changed as new autofill types are converted to USS.
  DCHECK_EQ(syncer::AUTOFILL, type_);
}

AutofillMetadataChangeList::~AutofillMetadataChangeList() {
  DCHECK(!error_);
}

void AutofillMetadataChangeList::UpdateModelTypeState(
    const sync_pb::ModelTypeState& model_type_state) {
  if (error_) {
    return;
  }

  if (!table_->UpdateModelTypeState(type_, model_type_state)) {
    error_ = ModelError(FROM_HERE, "Failed to update ModelTypeState.");
  }
}

void AutofillMetadataChangeList::ClearModelTypeState() {
  if (error_) {
    return;
  }

  if (!table_->ClearModelTypeState(type_)) {
    error_ = ModelError(FROM_HERE, "Failed to clear ModelTypeState.");
  }
}

void AutofillMetadataChangeList::UpdateMetadata(
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  if (error_) {
    return;
  }

  if (!table_->UpdateSyncMetadata(type_, storage_key, metadata)) {
    error_ = ModelError(FROM_HERE, "Failed to update entity metadata.");
  }
}

void AutofillMetadataChangeList::ClearMetadata(const std::string& storage_key) {
  if (error_) {
    return;
  }

  if (!table_->ClearSyncMetadata(type_, storage_key)) {
    error_ = ModelError(FROM_HERE, "Failed to clear entity metadata.");
  }
}

Optional<ModelError> AutofillMetadataChangeList::TakeError() {
  Optional<ModelError> temp = error_;
  error_.reset();
  return temp;
}

}  // namespace autofill
