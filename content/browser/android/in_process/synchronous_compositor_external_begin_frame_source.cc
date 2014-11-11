// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_external_begin_frame_source.h"

#include "cc/output/begin_frame_args.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

SynchronousCompositorExternalBeginFrameSource::
    SynchronousCompositorExternalBeginFrameSource(int routing_id)
    : routing_id_(routing_id),
      compositor_(nullptr) {
}

SynchronousCompositorExternalBeginFrameSource::
    ~SynchronousCompositorExternalBeginFrameSource() {
  DCHECK(CalledOnValidThread());
  if (compositor_)
    compositor_->DidDestroyExternalBeginFrameSource(this);
}

void SynchronousCompositorExternalBeginFrameSource::BeginFrame() {
  DCHECK(CalledOnValidThread());
  CallOnBeginFrame(cc::BeginFrameArgs::CreateForSynchronousCompositor());
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

  SynchronousCompositorImpl* compositor =
      SynchronousCompositorImpl::FromRoutingID(routing_id_);
  if (compositor) {
    compositor->DidInitializeExternalBeginFrameSource(this);
    compositor->NeedsBeginFramesChanged();
  }
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorExternalBeginFrameSource() must only be
// used on the UI thread.
bool
SynchronousCompositorExternalBeginFrameSource::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

}  // namespace content
