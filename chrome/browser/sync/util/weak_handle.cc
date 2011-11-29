// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/weak_handle.h"

#include <sstream>

#include "base/location.h"
#include "base/message_loop_proxy.h"

namespace browser_sync {

namespace internal {

WeakHandleCoreBase::WeakHandleCoreBase()
    : owner_loop_proxy_(base::MessageLoopProxy::current()) {}

bool WeakHandleCoreBase::IsOnOwnerThread() const {
  return owner_loop_proxy_->BelongsToCurrentThread();
}

WeakHandleCoreBase::~WeakHandleCoreBase() {}

void WeakHandleCoreBase::PostToOwnerThread(
    const tracked_objects::Location& from_here,
    const base::Closure& fn) const {
  if (!owner_loop_proxy_->PostTask(from_here, fn)) {
    DVLOG(1) << "Could not post task from " << from_here.ToString();
  }
}

}  // namespace internal

}  // namespace base
