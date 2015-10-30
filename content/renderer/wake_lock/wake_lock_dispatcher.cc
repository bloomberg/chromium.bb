// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/wake_lock/wake_lock_dispatcher.h"

#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"

namespace content {

WakeLockDispatcher::WakeLockDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame), blink::WebWakeLockClient() {}

WakeLockDispatcher::~WakeLockDispatcher() {}

void WakeLockDispatcher::requestKeepScreenAwake(bool keepScreenAwake) {
  if (!wake_lock_service_) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&wake_lock_service_));
  }

  if (keepScreenAwake) {
    wake_lock_service_->RequestWakeLock();
  } else {
    wake_lock_service_->CancelWakeLock();
  }
}

}  // namespace content
