// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_resize_lock.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "ui/compositor/compositor.h"

namespace content {

CompositorResizeLock::CompositorResizeLock(CompositorResizeLockClient* client,
                                           const gfx::Size& new_size)
    : client_(client),
      expected_size_(new_size),
      acquisition_time_(base::TimeTicks::Now()) {
  TRACE_EVENT_ASYNC_BEGIN2("ui", "CompositorResizeLock", this, "width",
                           expected_size().width(), "height",
                           expected_size().height());
}

CompositorResizeLock::~CompositorResizeLock() {
  compositor_lock_ = nullptr;
  if (client_)
    client_->CompositorResizeLockEnded();

  TRACE_EVENT_ASYNC_END2("ui", "CompositorResizeLock", this, "width",
                         expected_size().width(), "height",
                         expected_size().height());

  UMA_HISTOGRAM_TIMES("UI.CompositorResizeLock.Duration",
                      base::TimeTicks::Now() - acquisition_time_);

  UMA_HISTOGRAM_BOOLEAN("UI.CompositorResizeLock.TimedOut", timed_out_);
}

bool CompositorResizeLock::Lock() {
  if (unlocked_ || compositor_lock_)
    return false;
  compositor_lock_ = client_->GetCompositorLock(this);
  return true;
}

void CompositorResizeLock::UnlockCompositor() {
  unlocked_ = true;
  compositor_lock_ = nullptr;
}

void CompositorResizeLock::CompositorLockTimedOut() {
  timed_out_ = true;
  UnlockCompositor();
  if (client_) {
    client_->CompositorResizeLockEnded();
    client_ = nullptr;
  }
}

}  // namespace content
