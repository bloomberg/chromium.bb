// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_context_support.h"

#include "base/message_loop/message_loop.h"

namespace cc {

TestContextSupport::TestContextSupport() {}
TestContextSupport::~TestContextSupport() {}

void TestContextSupport::SignalSyncPoint(uint32 sync_point,
                                         const base::Closure& callback) {
  sync_point_callbacks_.push_back(callback);
}

void TestContextSupport::SignalQuery(uint32 query,
                                     const base::Closure& callback) {
  sync_point_callbacks_.push_back(callback);
}

void TestContextSupport::SetSurfaceVisible(bool visible) {
  if (!set_visible_callback_.is_null()) {
    set_visible_callback_.Run(visible);
  }
}

void TestContextSupport::SendManagedMemoryStats(
    const gpu::ManagedMemoryStats& stats) {}

void TestContextSupport::CallAllSyncPointCallbacks() {
  for (size_t i = 0; i < sync_point_callbacks_.size(); ++i) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, sync_point_callbacks_[i]);
  }
  sync_point_callbacks_.clear();
}

void TestContextSupport::SetSurfaceVisibleCallback(
    const SurfaceVisibleCallback& set_visible_callback) {
  set_visible_callback_ = set_visible_callback;
}

}  // namespace cc
