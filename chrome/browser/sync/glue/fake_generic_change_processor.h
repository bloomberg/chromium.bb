// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_FAKE_GENERIC_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_FAKE_GENERIC_CHANGE_PROCESSOR_H_
#pragma once

#include "chrome/browser/sync/glue/generic_change_processor.h"

#include "sync/api/sync_error.h"

namespace browser_sync {

// A fake GenericChangeProcessor that can return arbitrary values.
class FakeGenericChangeProcessor : public GenericChangeProcessor {
 public:
  FakeGenericChangeProcessor();
  virtual ~FakeGenericChangeProcessor();

  // Setters for GenericChangeProcessor implementation results.
  void set_process_sync_changes_error(const SyncError& error);
  void set_get_sync_data_for_type_error(const SyncError& error);
  void set_sync_model_has_user_created_nodes(bool has_nodes);
  void set_sync_model_has_user_created_nodes_success(bool success);
  void set_crypto_ready_if_necessary(bool crypto_ready);

  // GenericChangeProcessor implementations.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;
  virtual SyncError GetSyncDataForType(
      syncable::ModelType type, SyncDataList* current_sync_data) OVERRIDE;
  virtual bool SyncModelHasUserCreatedNodes(syncable::ModelType type,
                                            bool* has_nodes) OVERRIDE;
  virtual bool CryptoReadyIfNecessary(syncable::ModelType type) OVERRIDE;

 private:
  SyncError process_sync_changes_error_;
  SyncError get_sync_data_for_type_error_;
  bool sync_model_has_user_created_nodes_;
  bool sync_model_has_user_created_nodes_success_;
  bool crypto_ready_if_necessary_;
  syncable::ModelType type_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_FAKE_GENERIC_CHANGE_PROCESSOR_H_
