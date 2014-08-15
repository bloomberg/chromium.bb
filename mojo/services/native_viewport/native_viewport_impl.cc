// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_impl.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
#include "ui/events/event.h"

namespace mojo {
namespace {

bool IsRateLimitedEventType(ui::Event* event) {
  return event->type() == ui::ET_MOUSE_MOVED ||
         event->type() == ui::ET_MOUSE_DRAGGED ||
         event->type() == ui::ET_TOUCH_MOVED;
}

}  // namespace

NativeViewportImpl::NativeViewportImpl()
      : widget_(gfx::kNullAcceleratedWidget),
        waiting_for_event_ack_(false),
        weak_factory_(this) {}

NativeViewportImpl::~NativeViewportImpl() {
  // Destroy the NativeViewport early on as it may call us back during
  // destruction and we want to be in a known state.
  platform_viewport_.reset();
}

void NativeViewportImpl::Create(RectPtr bounds) {
  platform_viewport_ = PlatformViewport::Create(this);
  platform_viewport_->Init(bounds.To<gfx::Rect>());
  client()->OnCreated();
  OnBoundsChanged(bounds.To<gfx::Rect>());
}

void NativeViewportImpl::Show() {
  platform_viewport_->Show();
}

void NativeViewportImpl::Hide() {
  platform_viewport_->Hide();
}

void NativeViewportImpl::Close() {
  command_buffer_.reset();
  DCHECK(platform_viewport_);
  platform_viewport_->Close();
}

void NativeViewportImpl::SetBounds(RectPtr bounds) {
  platform_viewport_->SetBounds(bounds.To<gfx::Rect>());
}

void NativeViewportImpl::CreateGLES2Context(
    InterfaceRequest<CommandBuffer> command_buffer_request) {
  if (command_buffer_ || command_buffer_request_.is_pending()) {
    LOG(ERROR) << "Can't create multiple contexts on a NativeViewport";
    return;
  }
  command_buffer_request_ = command_buffer_request.Pass();
  CreateCommandBufferIfNeeded();
}

void NativeViewportImpl::OnBoundsChanged(const gfx::Rect& bounds) {
  CreateCommandBufferIfNeeded();
  client()->OnBoundsChanged(Rect::From(bounds));
}

void NativeViewportImpl::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  CreateCommandBufferIfNeeded();
}

bool NativeViewportImpl::OnEvent(ui::Event* ui_event) {
  // Must not return early before updating capture.
  switch (ui_event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
      platform_viewport_->SetCapture();
      break;
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_TOUCH_RELEASED:
      platform_viewport_->ReleaseCapture();
      break;
    default:
      break;
  }

  if (waiting_for_event_ack_ && IsRateLimitedEventType(ui_event))
    return false;

  client()->OnEvent(
      TypeConverter<EventPtr, ui::Event>::ConvertFrom(*ui_event),
      base::Bind(&NativeViewportImpl::AckEvent,
                 weak_factory_.GetWeakPtr()));
  waiting_for_event_ack_ = true;
  return false;
}

void NativeViewportImpl::OnDestroyed() {
  client()->OnDestroyed(base::Bind(&NativeViewportImpl::AckDestroyed,
                                   base::Unretained(this)));
}

void NativeViewportImpl::AckEvent() {
  waiting_for_event_ack_ = false;
}

void NativeViewportImpl::CreateCommandBufferIfNeeded() {
  if (!command_buffer_request_.is_pending())
    return;
  DCHECK(!command_buffer_.get());
  if (widget_ == gfx::kNullAcceleratedWidget)
    return;
  gfx::Size size = platform_viewport_->GetSize();
  if (size.IsEmpty())
    return;
  command_buffer_.reset(
      new CommandBufferImpl(widget_, platform_viewport_->GetSize()));
  WeakBindToRequest(command_buffer_.get(), &command_buffer_request_);
}

void NativeViewportImpl::AckDestroyed() {
  command_buffer_.reset();
}

}  // namespace mojo

