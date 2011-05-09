// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
#pragma once

#include "chrome/browser/sync/glue/change_processor.h"

class SyncableService;

namespace browser_sync {

// TODO(sync): Have this become a SyncableService implementation and deprecate
// ChangeProcessor.
// For now, it acts as a dummy change processor that just passes all
// ApplySyncChanges commands onto the local SyncableService..
class GenericChangeProcessor : public ChangeProcessor {
 public:
  GenericChangeProcessor(SyncableService* local_service,
                         UnrecoverableErrorHandler* error_handler);
  ~GenericChangeProcessor();

  // ChangeProcessor interface.
  // Just passes all arguments onto the |local_service_|
  virtual void ApplyChangesFromSyncModel(
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count);
 protected:
  // ChangeProcessor interface.
  virtual void StartImpl(Profile* profile);  // Not implemented.
  virtual void StopImpl();  // Not implemented.
 private:
  SyncableService* local_service_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_GENERIC_CHANGE_PROCESSOR_H_
