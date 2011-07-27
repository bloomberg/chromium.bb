// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/weak_handle.h"

namespace browser_sync {

namespace internal {

WeakHandleCoreBase::WeakHandleCoreBase()
    : message_loop_proxy_(
        base::MessageLoopProxy::CreateForCurrentThread()) {}

WeakHandleCoreBase::~WeakHandleCoreBase() {}

bool WeakHandleCoreBase::IsOnOwnerThread() const {
  return message_loop_proxy_->BelongsToCurrentThread();
}

void WeakHandleCoreBase::PostOnOwnerThread(
    const tracked_objects::Location& from_here,
    const base::Closure& fn) const {
  ignore_result(message_loop_proxy_->PostTask(from_here, fn));
}

}  // namespace internal

}  // namespace base
