// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/generic_change_processor_factory.h"

#include "components/sync_driver/generic_change_processor.h"
#include "sync/api/syncable_service.h"

namespace sync_driver {


GenericChangeProcessorFactory::GenericChangeProcessorFactory() {}

GenericChangeProcessorFactory::~GenericChangeProcessorFactory() {}

scoped_ptr<GenericChangeProcessor>
GenericChangeProcessorFactory::CreateGenericChangeProcessor(
    syncer::ModelType type,
    syncer::UserShare* user_share,
    DataTypeErrorHandler* error_handler,
    const base::WeakPtr<syncer::SyncableService>& local_service,
    const base::WeakPtr<syncer::SyncMergeResult>& merge_result,
    SyncApiComponentFactory* sync_factory) {
  DCHECK(user_share);
  return make_scoped_ptr(new GenericChangeProcessor(
                             type,
                             error_handler,
                             local_service,
                             merge_result,
                             user_share,
                             sync_factory,
                             local_service->GetAttachmentStore())).Pass();
}

}  // namespace sync_driver
