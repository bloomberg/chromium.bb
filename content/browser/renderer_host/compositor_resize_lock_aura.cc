// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_resize_lock_aura.h"

#include "base/debug/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/compositor.h"

namespace content {

CompositorResizeLock::CompositorResizeLock(aura::RootWindow* root_window,
                                           const gfx::Size new_size,
                                           bool defer_compositor_lock,
                                           const base::TimeDelta& timeout)
    : ResizeLock(new_size, defer_compositor_lock),
      root_window_(root_window),
      weak_ptr_factory_(this),
      cancelled_(false) {
  DCHECK(root_window_);

  TRACE_EVENT_ASYNC_BEGIN2("ui", "CompositorResizeLock", this,
                           "width", expected_size().width(),
                           "height", expected_size().height());
  root_window_->HoldPointerMoves();

  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CompositorResizeLock::CancelLock,
                 weak_ptr_factory_.GetWeakPtr()),
      timeout);
}

CompositorResizeLock::~CompositorResizeLock() {
  CancelLock();
  TRACE_EVENT_ASYNC_END2("ui", "CompositorResizeLock", this,
                         "width", expected_size().width(),
                         "height", expected_size().height());
}

bool CompositorResizeLock::GrabDeferredLock() {
  return ResizeLock::GrabDeferredLock();
}

void CompositorResizeLock::UnlockCompositor() {
  ResizeLock::UnlockCompositor();
  compositor_lock_ = NULL;
}

void CompositorResizeLock::LockCompositor() {
  ResizeLock::LockCompositor();
  compositor_lock_ = root_window_->host()->compositor()->GetCompositorLock();
}

void CompositorResizeLock::CancelLock() {
  if (cancelled_)
    return;
  cancelled_ = true;
  UnlockCompositor();
  root_window_->ReleasePointerMoves();
}

}  // namespace content
