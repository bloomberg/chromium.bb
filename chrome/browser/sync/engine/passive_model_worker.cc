// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/passive_model_worker.h"

#include "base/message_loop.h"

namespace browser_sync {

PassiveModelWorker::PassiveModelWorker(const MessageLoop* sync_loop)
    : sync_loop_(sync_loop) {}

PassiveModelWorker::~PassiveModelWorker() {
}

UnrecoverableErrorInfo PassiveModelWorker::DoWorkAndWaitUntilDone(
    const WorkCallback& work) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  // Simply do the work on the current thread.
  return work.Run();
}

ModelSafeGroup PassiveModelWorker::GetModelSafeGroup() {
  return GROUP_PASSIVE;
}

}  // namespace browser_sync
