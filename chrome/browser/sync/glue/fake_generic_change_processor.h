// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_FAKE_GENERIC_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_FAKE_GENERIC_CHANGE_PROCESSOR_H_

#include "chrome/browser/sync/glue/generic_change_processor.h"

#include "sync/api/sync_error.h"

namespace browser_sync {

// A fake GenericChangeProcessor that can return arbitrary values.
class FakeGenericChangeProcessor : public GenericChangeProcessor {
 public:
  FakeGenericChangeProcessor();
  virtual ~FakeGenericChangeProcessor();

  // Setters for GenericChangeProcessor implementation results.
  void set_process_sync_changes_error(const syncer::SyncError& error);
  void set_get_sync_data_for_type_error(const syncer::SyncError& error);
  void set_sync_model_has_user_created_nodes(bool has_nodes);
  void set_sync_model_has_user_created_nodes_success(bool success);
  void set_crypto_ready_if_necessary(bool crypto_ready);

  // GenericChangeProcessor implementations.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;
  virtual syncer::SyncError GetAllSyncDataReturnError(
      syncer::ModelType type,
      syncer::SyncDataList* data) const OVERRIDE;
  virtual bool GetDataTypeContext(syncer::ModelType type,
                                  std::string* context) const OVERRIDE;
  virtual int GetSyncCountForType(syncer::ModelType type) OVERRIDE;
  virtual bool SyncModelHasUserCreatedNodes(syncer::ModelType type,
                                            bool* has_nodes) OVERRIDE;
  virtual bool CryptoReadyIfNecessary(syncer::ModelType type) OVERRIDE;

 private:
  syncer::SyncError process_sync_changes_error_;
  syncer::SyncError get_sync_data_for_type_error_;
  bool sync_model_has_user_created_nodes_;
  bool sync_model_has_user_created_nodes_success_;
  bool crypto_ready_if_necessary_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_FAKE_GENERIC_CHANGE_PROCESSOR_H_
