// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_impl.h"

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "mojo/services/gles2/gles2_impl.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "ui/events/event.h"

namespace mojo {
namespace services {
namespace {

bool IsRateLimitedEventType(ui::Event* event) {
  return event->type() == ui::ET_MOUSE_MOVED ||
         event->type() == ui::ET_MOUSE_DRAGGED ||
         event->type() == ui::ET_TOUCH_MOVED;
}

}

////////////////////////////////////////////////////////////////////////////////
// NativeViewportImpl, public:

NativeViewportImpl::NativeViewportImpl(shell::Context* context,
                                       ScopedMessagePipeHandle pipe)
    : context_(context),
      widget_(gfx::kNullAcceleratedWidget),
      waiting_for_event_ack_(false),
      pending_event_timestamp_(0),
      client_(pipe.Pass()) {
  client_.SetPeer(this);
}

NativeViewportImpl::~NativeViewportImpl() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewportImpl, NativeViewportStub overrides:

void NativeViewportImpl::Open() {
  native_viewport_ = services::NativeViewport::Create(context_, this);
  native_viewport_->Init();
  client_->OnCreated();
}

void NativeViewportImpl::Close() {
  DCHECK(native_viewport_);
  native_viewport_->Close();
}

void NativeViewportImpl::CreateGLES2Context(
    ScopedMessagePipeHandle gles2_client) {
  gles2_.reset(new GLES2Impl(gles2_client.Pass()));
  CreateGLES2ContextIfNeeded();
}

void NativeViewportImpl::AckEvent(const Event& event) {
  DCHECK_EQ(event.time_stamp(), pending_event_timestamp_);
  waiting_for_event_ack_ = false;
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewportImpl, NativeViewportDelegate implementation:

void NativeViewportImpl::OnResized(const gfx::Size& size) {
}

void NativeViewportImpl::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  CreateGLES2ContextIfNeeded();
}

bool NativeViewportImpl::OnEvent(ui::Event* ui_event) {
  // Must not return early before updating capture.
  switch (ui_event->type()) {
  case ui::ET_MOUSE_PRESSED:
  case ui::ET_TOUCH_PRESSED:
    native_viewport_->SetCapture();
    break;
  case ui::ET_MOUSE_RELEASED:
  case ui::ET_TOUCH_RELEASED:
    native_viewport_->ReleaseCapture();
    break;
  default:
    break;
  }

  if (waiting_for_event_ack_ && IsRateLimitedEventType(ui_event))
    return false;

  AllocationScope scope;

  Event::Builder event;
  event.set_action(ui_event->type());
  pending_event_timestamp_ = ui_event->time_stamp().ToInternalValue();
  event.set_time_stamp(pending_event_timestamp_);

  if (ui_event->IsMouseEvent() || ui_event->IsTouchEvent()) {
    ui::LocatedEvent* located_event = static_cast<ui::LocatedEvent*>(ui_event);
    Point::Builder location;
    location.set_x(located_event->location().x());
    location.set_y(located_event->location().y());
    event.set_location(location.Finish());
  }

  if (ui_event->IsTouchEvent()) {
    ui::TouchEvent* touch_event = static_cast<ui::TouchEvent*>(ui_event);
    TouchData::Builder touch_data;
    touch_data.set_pointer_id(touch_event->touch_id());
    event.set_touch_data(touch_data.Finish());
  }

  client_->OnEvent(event.Finish());
  waiting_for_event_ack_ = true;
  return false;
}

void NativeViewportImpl::OnDestroyed() {
  // TODO(beng):
  // Destroying |gles2_| on the shell thread here hits thread checker asserts.
  // All code must stop touching the AcceleratedWidget at this point as it is
  // dead after this call stack. jamesr said we probably should make our own
  // GLSurface and simply tell it to stop touching the AcceleratedWidget
  // via Destroy() but we have no good way of doing that right now given our
  // current threading model so james' recommendation was just to wait until
  // after we move the gl service out of process.
  // gles2_.reset();
  client_->OnDestroyed();
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewportImpl, private:

void NativeViewportImpl::CreateGLES2ContextIfNeeded() {
  if (widget_ == gfx::kNullAcceleratedWidget || !gles2_)
    return;
  gles2_->CreateContext(widget_, native_viewport_->GetSize());
}

}  // namespace services
}  // namespace mojo
