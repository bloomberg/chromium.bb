// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_operation.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_operation_manager.h"

namespace extensions {
namespace recovery {

namespace {

recovery_api::Stage nextStage(recovery_api::Stage stage) {
  if (stage == recovery_api::STAGE_CONFIRMATION) {
    return recovery_api::STAGE_DOWNLOAD;
  } else if (stage == recovery_api::STAGE_DOWNLOAD) {
    return recovery_api::STAGE_VERIFYDOWNLOAD;
  } else if (stage == recovery_api::STAGE_VERIFYDOWNLOAD) {
    return recovery_api::STAGE_WRITE;
  } else if (stage == recovery_api::STAGE_WRITE) {
    return recovery_api::STAGE_VERIFYWRITE;
  } else {
    return recovery_api::STAGE_NONE;
  }
}

} // namespace

RecoveryOperation::RecoveryOperation(RecoveryOperationManager* manager,
                                     const ExtensionId& extension_id)
    : manager_(manager), extension_id_(extension_id) {
}

RecoveryOperation::~RecoveryOperation() {
  timer_.Stop();
}

void RecoveryOperation::Start(const WriteStartCallback& callback) {
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(100),
               this,
               &RecoveryOperation::UpdateProgress);

  stage_ = recovery_api::STAGE_DOWNLOAD;
  progress_ = 0;

  callback.Run(true);
}

void RecoveryOperation::Cancel(const WriteCancelCallback& callback) {
  timer_.Stop();
  callback.Run(true);
}

void RecoveryOperation::Abort() {
  timer_.Stop();
  manager_->OnError(extension_id_, stage_, progress_);
}

void RecoveryOperation::UpdateProgress() {
  if (stage_ == recovery_api::STAGE_NONE) {
    return;
  }

  progress_ += 10;

  if (progress_ > 100) {
    progress_ = 0;
    stage_ = nextStage(stage_);

    if (stage_ == recovery_api::STAGE_NONE) {
      timer_.Stop();
      manager_->OnComplete(extension_id_);
      return;
    }
  }

  manager_->OnProgress(extension_id_, stage_, progress_);
}


} // namespace recovery
} // namespace extensions
