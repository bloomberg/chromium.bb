// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/avda_shared_state.h"

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
  frame_available_event_.Wait();
}

void AVDASharedState::did_attach_surface_texture() {
  context_ = gfx::GLContext::GetCurrent();
  surface_ = gfx::GLSurface::GetCurrent();
  DCHECK(context_);
  DCHECK(surface_);

  surface_texture_is_attached_ = true;
}

}  // namespace content
