// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/generic_change_processor_factory.h"

#include "components/sync_driver/generic_change_processor.h"

namespace browser_sync {


GenericChangeProcessorFactory::GenericChangeProcessorFactory() {}

GenericChangeProcessorFactory::~GenericChangeProcessorFactory() {}

scoped_ptr<GenericChangeProcessor>
GenericChangeProcessorFactory::CreateGenericChangeProcessor(
      syncer::UserShare* user_share,
      browser_sync::DataTypeErrorHandler* error_handler,
      const base::WeakPtr<syncer::SyncableService>& local_service,
      const base::WeakPtr<syncer::SyncMergeResult>& merge_result,
      scoped_ptr<syncer::AttachmentService> attachment_service) {
    return make_scoped_ptr(new GenericChangeProcessor(
        error_handler,
        local_service,
        merge_result,
        user_share,
        attachment_service.Pass())).Pass();
  }

}  // namespace browser_sync
