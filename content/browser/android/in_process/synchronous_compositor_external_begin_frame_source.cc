// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_external_begin_frame_source.h"

#include "cc/output/begin_frame_args.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/browser/android/in_process/synchronous_compositor_registry.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/frame_time.h"

namespace content {

SynchronousCompositorExternalBeginFrameSource::
    SynchronousCompositorExternalBeginFrameSource(int routing_id)
    : routing_id_(routing_id),
      registered_(false),
      compositor_(nullptr) {
}

SynchronousCompositorExternalBeginFrameSource::
    ~SynchronousCompositorExternalBeginFrameSource() {
  DCHECK(CalledOnValidThread());

  if (registered_) {
    SynchronousCompositorRegistry::GetInstance()->UnregisterBeginFrameSource(
        routing_id_, this);
  }
  DCHECK(!compositor_);
}

void SynchronousCompositorExternalBeginFrameSource::BeginFrame() {
  DCHECK(CalledOnValidThread());
  CallOnBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, gfx::FrameTime::Now(), base::TimeTicks(),
      cc::BeginFrameArgs::DefaultInterval(), cc::BeginFrameArgs::SYNCHRONOUS));
}

void SynchronousCompositorExternalBeginFrameSource::SetCompositor(
    SynchronousCompositorImpl* compositor) {
  DCHECK(CalledOnValidThread());
  compositor_ = compositor;
}

void SynchronousCompositorExternalBeginFrameSource::OnNeedsBeginFramesChange(
    bool needs_begin_frames) {
  DCHECK(CalledOnValidThread());

  if (compositor_)
    compositor_->NeedsBeginFramesChanged();
}

void SynchronousCompositorExternalBeginFrameSource::SetClientReady() {
  DCHECK(CalledOnValidThread());
  SynchronousCompositorRegistry::GetInstance()->RegisterBeginFrameSource(
      routing_id_, this);
  registered_ = true;
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorExternalBeginFrameSource() must only be
// used on the UI thread.
bool
SynchronousCompositorExternalBeginFrameSource::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

}  // namespace content
