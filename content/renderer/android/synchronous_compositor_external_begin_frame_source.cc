// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_external_begin_frame_source.h"

#include "cc/output/begin_frame_args.h"
#include "content/renderer/android/synchronous_compositor_registry.h"

namespace content {

SynchronousCompositorExternalBeginFrameSource::
    SynchronousCompositorExternalBeginFrameSource(
        int routing_id,
        SynchronousCompositorRegistry* registry)
    : routing_id_(routing_id),
      registry_(registry),
      registered_(false),
      client_(nullptr) {
  thread_checker_.DetachFromThread();
}

SynchronousCompositorExternalBeginFrameSource::
    ~SynchronousCompositorExternalBeginFrameSource() {
  DCHECK(CalledOnValidThread());

  if (registered_) {
    registry_->UnregisterBeginFrameSource(routing_id_, this);
  }
  DCHECK(!client_);
}

void SynchronousCompositorExternalBeginFrameSource::BeginFrame(
    const cc::BeginFrameArgs& args) {
  DCHECK(CalledOnValidThread());
  CallOnBeginFrame(args);
}

void SynchronousCompositorExternalBeginFrameSource::SetClient(
    SynchronousCompositorExternalBeginFrameSourceClient* client) {
  DCHECK(CalledOnValidThread());
  if (client_ == client)
    return;

  if (client_)
    client_->OnNeedsBeginFramesChange(false);

  client_ = client;

  if (client_)
    client_->OnNeedsBeginFramesChange(needs_begin_frames());

  // State without client is paused, and default client state is not paused.
  SetBeginFrameSourcePaused(!client_);
}

void SynchronousCompositorExternalBeginFrameSource::OnNeedsBeginFramesChanged(
    bool needs_begin_frames) {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->OnNeedsBeginFramesChange(needs_begin_frames);
}

void SynchronousCompositorExternalBeginFrameSource::SetClientReady() {
  DCHECK(CalledOnValidThread());
  registry_->RegisterBeginFrameSource(routing_id_, this);
  registered_ = true;
}

bool
SynchronousCompositorExternalBeginFrameSource::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

}  // namespace content
