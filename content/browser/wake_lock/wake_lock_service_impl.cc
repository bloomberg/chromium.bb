// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/wake_lock/wake_lock_service_impl.h"

#include "content/browser/wake_lock/wake_lock_service_context.h"

namespace content {

WakeLockServiceImpl::WakeLockServiceImpl(
    base::WeakPtr<WakeLockServiceContext> context,
    int frame_id,
    mojo::InterfaceRequest<WakeLockService> request)
    : context_(context),
      frame_id_(frame_id),
      binding_(this, request.Pass()) {
}

WakeLockServiceImpl::~WakeLockServiceImpl() {
}

void WakeLockServiceImpl::RequestWakeLock() {
  if (context_)
    context_->RequestWakeLock(frame_id_);
}

void WakeLockServiceImpl::CancelWakeLock() {
  if (context_)
    context_->CancelWakeLock(frame_id_);
}

}  // namespace content
