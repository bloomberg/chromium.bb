// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/avda_shared_state.h"

#include "base/time/time.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_make_current.h"

namespace content {

AVDASharedState::AVDASharedState()
    : surface_texture_service_id_(0),
      frame_available_event_(false, false),
      surface_texture_is_attached_(false) {}

AVDASharedState::~AVDASharedState() {}

void AVDASharedState::SignalFrameAvailable() {
  frame_available_event_.Signal();
}

void AVDASharedState::WaitForFrameAvailable() {
  // 10msec covers >99.9% of cases, so just wait for up to that much before
  // giving up.  If an error occurs, we might not ever get a notification.
  const base::TimeDelta max_wait_time(base::TimeDelta::FromMilliseconds(10));
  frame_available_event_.TimedWait(max_wait_time);
}

void AVDASharedState::DidAttachSurfaceTexture() {
  context_ = gfx::GLContext::GetCurrent();
  surface_ = gfx::GLSurface::GetCurrent();
  DCHECK(context_);
  DCHECK(surface_);

  surface_texture_is_attached_ = true;
}

void AVDASharedState::DidDetachSurfaceTexture() {
  context_ = nullptr;
  surface_ = nullptr;
  surface_texture_is_attached_ = false;
}

}  // namespace content
