// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_REF_H_
#define CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_REF_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/glue/shared_change_processor.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"

namespace browser_sync {

// A SyncChangeProcessor stub for interacting with a refcounted
// SharedChangeProcessor.
class SharedChangeProcessorRef : public SyncChangeProcessor,
                                 public SyncErrorFactory {
 public:
  SharedChangeProcessorRef(
      const scoped_refptr<browser_sync::SharedChangeProcessor>&
          change_processor);
  virtual ~SharedChangeProcessorRef();

  // SyncChangeProcessor implementation.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;

  // SyncErrorFactory implementation.
  virtual SyncError CreateAndUploadError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;

  // Default copy and assign welcome (and safe due to refcounted-ness).

 private:
  scoped_refptr<browser_sync::SharedChangeProcessor> change_processor_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SHARED_CHANGE_PROCESSOR_REF_H_
