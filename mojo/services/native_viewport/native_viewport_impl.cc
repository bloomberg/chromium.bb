// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_impl.h"

#include "base/message_loop/message_loop.h"
#include "mojo/services/gles2/gles2_impl.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "ui/events/event.h"

namespace mojo {
namespace services {

NativeViewportImpl::NativeViewportImpl(shell::Context* context,
                                       ScopedMessagePipeHandle pipe)
    : context_(context),
      widget_(gfx::kNullAcceleratedWidget),
      client_(pipe.Pass()) {
  client_.SetPeer(this);
}

NativeViewportImpl::~NativeViewportImpl() {
}

void NativeViewportImpl::Open() {
  native_viewport_ = services::NativeViewport::Create(context_, this);
  native_viewport_->Init();
  client_->DidOpen();
}

void NativeViewportImpl::Close() {
  gles2_.reset();
  DCHECK(native_viewport_);
  native_viewport_->Close();
}

void NativeViewportImpl::CreateGLES2Context(
    ScopedMessagePipeHandle gles2_client) {
  gles2_.reset(new GLES2Impl(gles2_client.Pass()));
  CreateGLES2ContextIfNeeded();
}

void NativeViewportImpl::CreateGLES2ContextIfNeeded() {
  if (widget_ == gfx::kNullAcceleratedWidget || !gles2_)
    return;
  gles2_->CreateContext(widget_, native_viewport_->GetSize());
}

bool NativeViewportImpl::OnEvent(ui::Event* ui_event) {
  AllocationScope scope;

  Event::Builder event;
  event.set_action(ui_event->type());
  event.set_time_stamp(ui_event->time_stamp().ToInternalValue());

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

  client_->HandleEvent(event.Finish());
  return false;
}

void NativeViewportImpl::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  CreateGLES2ContextIfNeeded();
}

void NativeViewportImpl::OnResized(const gfx::Size& size) {
}

void NativeViewportImpl::OnDestroyed() {
  base::MessageLoop::current()->Quit();
}

}  // namespace services
}  // namespace mojo
