// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/change_processor.h"

namespace sync_driver {

ChangeProcessor::ChangeProcessor(DataTypeErrorHandler* error_handler)
    : error_handler_(error_handler),
      share_handle_(NULL) {}

ChangeProcessor::~ChangeProcessor() {
}

void ChangeProcessor::Start(syncer::UserShare* share_handle) {
  DCHECK(!share_handle_);
  share_handle_ = share_handle;
  StartImpl();
}

// Not implemented by default.
void ChangeProcessor::CommitChangesFromSyncModel() {}

DataTypeErrorHandler* ChangeProcessor::error_handler() const {
  return error_handler_;
}

syncer::UserShare* ChangeProcessor::share_handle() const {
  return share_handle_;
}

}  // namespace sync_driver
