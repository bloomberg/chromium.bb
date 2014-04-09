// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_REF_H_
#define CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_REF_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/glue/shared_change_processor.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"

namespace browser_sync {

// A syncer::SyncChangeProcessor stub for interacting with a refcounted
// SharedChangeProcessor.
class SharedChangeProcessorRef : public syncer::SyncChangeProcessor,
                                 public syncer::SyncErrorFactory {
 public:
  SharedChangeProcessorRef(
      const scoped_refptr<browser_sync::SharedChangeProcessor>&
          change_processor);
  virtual ~SharedChangeProcessorRef();

  // syncer::SyncChangeProcessor implementation.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError UpdateDataTypeContext(
      syncer::ModelType type,
      syncer::SyncChangeProcessor::ContextRefreshStatus refresh_status,
      const std::string& context) OVERRIDE;

  // syncer::SyncErrorFactory implementation.
  virtual syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;

  // Default copy and assign welcome (and safe due to refcounted-ness).

 private:
  scoped_refptr<browser_sync::SharedChangeProcessor> change_processor_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_REF_H_
