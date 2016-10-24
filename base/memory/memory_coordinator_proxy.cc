// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_coordinator_proxy.h"

namespace base {

MemoryCoordinatorProxy::MemoryCoordinatorProxy() {
}

MemoryCoordinatorProxy::~MemoryCoordinatorProxy() {
}

MemoryCoordinatorProxy* MemoryCoordinatorProxy::GetInstance() {
  return Singleton<base::MemoryCoordinatorProxy>::get();
}

MemoryState MemoryCoordinatorProxy::GetCurrentMemoryState() const {
  if (!callback_)
    return MemoryState::NORMAL;
  return callback_.Run();
}

void MemoryCoordinatorProxy::SetGetCurrentMemoryStateCallback(
    GetCurrentMemoryStateCallback callback) {
  callback_ = callback;
}

}  // namespace base
