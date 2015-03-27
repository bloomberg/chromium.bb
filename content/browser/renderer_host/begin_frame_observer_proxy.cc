// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/begin_frame_observer_proxy.h"

namespace content {

BeginFrameObserverProxy::BeginFrameObserverProxy(
    BeginFrameObserverProxyClient* client)
    : needs_begin_frames_(false),
      client_(client),
      compositor_(nullptr) {
}

BeginFrameObserverProxy::~BeginFrameObserverProxy() {
  DCHECK(!compositor_);
}

void BeginFrameObserverProxy::SetNeedsBeginFrames(bool needs_begin_frames) {
  if (needs_begin_frames_ == needs_begin_frames)
    return;

  needs_begin_frames_ = needs_begin_frames;

  // In some cases, BeginFrame message is requested before |client_|'s window is
  // added in the root window hierarchy.
  if (!compositor_)
    return;

  if (needs_begin_frames)
    StartObservingBeginFrames();
  else
    StopObservingBeginFrames();
}

void BeginFrameObserverProxy::SetCompositor(ui::Compositor* compositor) {
  DCHECK(!compositor_);
  DCHECK(compositor);

  compositor_ = compositor;
  compositor_->AddObserver(this);
  if (needs_begin_frames_)
    StartObservingBeginFrames();
}

void BeginFrameObserverProxy::ResetCompositor() {
  if (!compositor_)
    return;
  compositor_->RemoveObserver(this);

  if (needs_begin_frames_)
    StopObservingBeginFrames();
  compositor_ = nullptr;
}

void BeginFrameObserverProxy::OnSendBeginFrame(const cc::BeginFrameArgs& args) {
  if (last_sent_begin_frame_args_.frame_time != args.frame_time)
    client_->SendBeginFrame(args);
  last_sent_begin_frame_args_ = args;
}

void BeginFrameObserverProxy::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  ResetCompositor();
}

void BeginFrameObserverProxy::StartObservingBeginFrames() {
  DCHECK(compositor_);
  compositor_->AddBeginFrameObserver(this);
}

void BeginFrameObserverProxy::StopObservingBeginFrames() {
  DCHECK(compositor_);
  compositor_->RemoveBeginFrameObserver(this);
}

}  // namespace content
