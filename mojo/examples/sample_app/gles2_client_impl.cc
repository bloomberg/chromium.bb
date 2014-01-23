// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/sample_app/gles2_client_impl.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <math.h>

#include "mojo/public/gles2/gles2.h"
#include "ui/events/event_constants.h"

namespace mojo {
namespace examples {
namespace {

float CalculateDragDistance(const gfx::PointF& start, const Point& end) {
  return hypot(start.x() - end.x(), start.y() - end.y());
}

}

GLES2ClientImpl::GLES2ClientImpl(ScopedMessagePipeHandle pipe)
    : getting_animation_frames_(false) {
  context_ = MojoGLES2CreateContext(
      pipe.release().value(),
      &DidCreateContextThunk,
      &ContextLostThunk,
      &DrawAnimationFrameThunk,
      this);
}

GLES2ClientImpl::~GLES2ClientImpl() {
  MojoGLES2DestroyContext(context_);
}

void GLES2ClientImpl::HandleInputEvent(const Event& event) {
  switch (event.action()) {
  case ui::ET_MOUSE_PRESSED:
  case ui::ET_TOUCH_PRESSED:
    CancelAnimationFrames();
    capture_point_.SetPoint(event.location().x(), event.location().y());
    last_drag_point_ = capture_point_;
    drag_start_time_ = GetTimeTicksNow();
    break;
  case ui::ET_MOUSE_DRAGGED:
  case ui::ET_TOUCH_MOVED:
    if (!getting_animation_frames_) {
      int direction = event.location().y() < last_drag_point_.y() ||
          event.location().x() > last_drag_point_.x() ? 1 : -1;
      cube_.set_direction(direction);
      cube_.UpdateForDragDistance(
          CalculateDragDistance(last_drag_point_, event.location()));
      cube_.Draw();
      MojoGLES2SwapBuffers();

      last_drag_point_.SetPoint(event.location().x(), event.location().y());
    }
    break;
  case ui::ET_MOUSE_RELEASED:
  case ui::ET_TOUCH_RELEASED: {
      MojoTimeTicks offset = GetTimeTicksNow() - drag_start_time_;
      float delta = static_cast<float>(offset) / 1000000.;
      cube_.SetFlingMultiplier(
          CalculateDragDistance(capture_point_, event.location()),
          delta);

      capture_point_ = last_drag_point_ = gfx::PointF();
      RequestAnimationFrames();
    }
    break;
  default:
    break;
  }
}

void GLES2ClientImpl::DidCreateContext(uint32_t width,
                                       uint32_t height) {
  MojoGLES2MakeCurrent(context_);

  cube_.Init(width, height);
  RequestAnimationFrames();
}

void GLES2ClientImpl::DidCreateContextThunk(
    void* closure,
    uint32_t width,
    uint32_t height) {
  static_cast<GLES2ClientImpl*>(closure)->DidCreateContext(width, height);
}

void GLES2ClientImpl::ContextLost() {
  CancelAnimationFrames();
}

void GLES2ClientImpl::ContextLostThunk(void* closure) {
  static_cast<GLES2ClientImpl*>(closure)->ContextLost();
}

void GLES2ClientImpl::DrawAnimationFrame() {
  MojoTimeTicks now = GetTimeTicksNow();
  MojoTimeTicks offset = now - last_time_;
  float delta = static_cast<float>(offset) / 1000000.;
  last_time_ = now;
  cube_.UpdateForTimeDelta(delta);
  cube_.Draw();

  MojoGLES2SwapBuffers();
}

void GLES2ClientImpl::DrawAnimationFrameThunk(void* closure) {
  static_cast<GLES2ClientImpl*>(closure)->DrawAnimationFrame();
}

void GLES2ClientImpl::RequestAnimationFrames() {
  getting_animation_frames_ = true;
  MojoGLES2RequestAnimationFrames(context_);
  last_time_ = GetTimeTicksNow();
}

void GLES2ClientImpl::CancelAnimationFrames() {
  getting_animation_frames_ = false;
  MojoGLES2CancelAnimationFrames(context_);
}

}  // namespace examples
}  // namespace mojo
