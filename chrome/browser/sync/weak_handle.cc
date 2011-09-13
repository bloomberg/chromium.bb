// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/weak_handle.h"

#include <sstream>

#include "base/message_loop_proxy.h"
#include "base/tracked.h"

namespace browser_sync {

namespace internal {

WeakHandleCoreBase::WeakHandleCoreBase()
    : owner_loop_proxy_(base::MessageLoopProxy::current()) {}

bool WeakHandleCoreBase::IsOnOwnerThread() const {
  return owner_loop_proxy_->BelongsToCurrentThread();
}

WeakHandleCoreBase::~WeakHandleCoreBase() {}

namespace {

// TODO(akalin): Merge with similar function in
// js_transaction_observer.cc.
std::string GetLocationString(const tracked_objects::Location& location) {
  std::ostringstream oss;
  oss << location.function_name() << "@"
      << location.file_name() << ":" << location.line_number();
  return oss.str();
}

}  // namespace

void WeakHandleCoreBase::PostToOwnerThread(
    const tracked_objects::Location& from_here,
    const base::Closure& fn) const {
  if (!owner_loop_proxy_->PostTask(from_here, fn)) {
    VLOG(1) << "Could not post task from " << GetLocationString(from_here);
  }
}

}  // namespace internal

}  // namespace base
