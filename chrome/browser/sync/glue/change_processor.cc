// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/profiles/profile.h"

namespace browser_sync {

ChangeProcessor::ChangeProcessor(UnrecoverableErrorHandler* error_handler)
    : running_(false),
      error_handler_(error_handler),
      share_handle_(NULL) {}

ChangeProcessor::~ChangeProcessor() {
  DCHECK(!running_) << "ChangeProcessor dtor while running";
}

void ChangeProcessor::Start(Profile* profile,
                            sync_api::UserShare* share_handle) {
  DCHECK(error_handler_ && !share_handle_);
  share_handle_ = share_handle;
  StartImpl(profile);
  running_ = true;
}

void ChangeProcessor::Stop() {
  if (!running_)
    return;
  StopImpl();
  share_handle_ = NULL;
  running_ = false;
}

bool ChangeProcessor::IsRunning() const {
  return running_;
}

// Not implemented by default.
void ChangeProcessor::CommitChangesFromSyncModel() {}

UnrecoverableErrorHandler* ChangeProcessor::error_handler() {
  return error_handler_;
}

sync_api::UserShare* ChangeProcessor::share_handle() {
  return share_handle_;
}

}  // namespace browser_sync
