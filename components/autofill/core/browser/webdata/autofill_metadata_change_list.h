// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_METADATA_CHANGE_LIST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_METADATA_CHANGE_LIST_H_

#include <string>

#include "base/optional.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace autofill {

// A thin wrapper around an AutofillTable that implements sync's
// MetadataChangeList interface. Changes are passed directly into the table and
// not stored inside this object. Since the table calls can fail, |TakeError()|
// must be called before this object is destroyed to check whether any
// operations failed.
class AutofillMetadataChangeList : public syncer::MetadataChangeList {
 public:
  AutofillMetadataChangeList(AutofillTable* table, syncer::ModelType type);
  ~AutofillMetadataChangeList() override;

  // syncer::MetadataChangeList implementation.
  void UpdateModelTypeState(
      const sync_pb::ModelTypeState& model_type_state) override;
  void ClearModelTypeState() override;
  void UpdateMetadata(const std::string& storage_key,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& storage_key) override;

  // Returns the value of |error_| and unsets it.
  base::Optional<syncer::ModelError> TakeError();

 private:
  // The autofill table to store metadata in; always outlives |this|.
  AutofillTable* table_;

  // The sync model type for this metadata.
  syncer::ModelType type_;

  // The first error encountered by this object, if any.
  base::Optional<syncer::ModelError> error_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_METADATA_CHANGE_LIST_H_
