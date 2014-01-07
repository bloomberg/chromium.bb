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
    : service_(pipe.Pass(), this) {
}

GLES2ClientImpl::~GLES2ClientImpl() {
  service_->Destroy();
}

void GLES2ClientImpl::HandleInputEvent(const Event& event) {
  switch (event.action()) {
  case ui::ET_MOUSE_PRESSED:
  case ui::ET_TOUCH_PRESSED:
    timer_.Stop();
    capture_point_.SetPoint(event.location().x(), event.location().y());
    last_drag_point_ = capture_point_;
    drag_start_time_ = base::Time::Now();
    break;
  case ui::ET_MOUSE_DRAGGED:
  case ui::ET_TOUCH_MOVED:
    if (!timer_.IsRunning()) {
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
      cube_.SetFlingMultiplier(
          CalculateDragDistance(capture_point_, event.location()),
          base::TimeDelta(base::Time::Now() - drag_start_time_).InSecondsF());

      capture_point_ = last_drag_point_ = gfx::PointF();
      StartTimer();
    }
    break;
  default:
    break;
  }
}

void GLES2ClientImpl::DidCreateContext(uint64_t encoded,
                                       uint32_t width,
                                       uint32_t height) {
  MojoGLES2MakeCurrent(encoded);

  cube_.Init(width, height);
  StartTimer();
}

void GLES2ClientImpl::ContextLost() {
  timer_.Stop();
}

void GLES2ClientImpl::Draw() {
  base::Time now = base::Time::Now();
  base::TimeDelta offset = now - last_time_;
  last_time_ = now;
  cube_.UpdateForTimeDelta(offset.InSecondsF());
  cube_.Draw();

  MojoGLES2SwapBuffers();
}

void GLES2ClientImpl::StartTimer() {
  last_time_ = base::Time::Now();
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(16),
               this, &GLES2ClientImpl::Draw);
}

}  // namespace examples
}  // namespace mojo
