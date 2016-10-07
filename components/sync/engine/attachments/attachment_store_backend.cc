// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/attachments/attachment_store_backend.h"

#include "base/location.h"
#include "base/sequenced_task_runner.h"

namespace syncer {

AttachmentStoreBackend::AttachmentStoreBackend(
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner)
    : callback_task_runner_(callback_task_runner) {}

AttachmentStoreBackend::~AttachmentStoreBackend() {}

void AttachmentStoreBackend::PostCallback(const base::Closure& callback) {
  callback_task_runner_->PostTask(FROM_HERE, callback);
}

}  // namespace syncer
