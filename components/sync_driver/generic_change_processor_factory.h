// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GENERIC_CHANGE_PROCESSOR_FACTORY_H_
#define COMPONENTS_SYNC_DRIVER_GENERIC_CHANGE_PROCESSOR_FACTORY_H_

#include "base/memory/weak_ptr.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {
class AttachmentService;
class SyncableService;
class SyncMergeResult;
struct UserShare;
}

namespace sync_driver {

class DataTypeErrorHandler;
class GenericChangeProcessor;
class SyncApiComponentFactory;

// Because GenericChangeProcessors are created and used only from the model
// thread, their lifetime is strictly shorter than other components like
// DataTypeController, which live before / after communication with model
// threads begins and ends.
// The GCP is created "on the fly" at just the right time, on just the right
// thread. Given that, we use a factory to instantiate GenericChangeProcessors
// so that tests can choose to use a fake processor (i.e instead of injection).
// |sync_factory| is used to create AttachmentServicefor GenericChangeProcessor.
// It is not retained after CreateGenericChangeProcessor exits.
class GenericChangeProcessorFactory {
 public:
  GenericChangeProcessorFactory();
  virtual ~GenericChangeProcessorFactory();
  virtual scoped_ptr<GenericChangeProcessor> CreateGenericChangeProcessor(
      syncer::ModelType type,
      syncer::UserShare* user_share,
      DataTypeErrorHandler* error_handler,
      const base::WeakPtr<syncer::SyncableService>& local_service,
      const base::WeakPtr<syncer::SyncMergeResult>& merge_result,
      SyncApiComponentFactory* sync_factory);

 private:
  DISALLOW_COPY_AND_ASSIGN(GenericChangeProcessorFactory);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_GENERIC_CHANGE_PROCESSOR_FACTORY_H_
