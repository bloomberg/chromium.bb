// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_impl.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/services/native_viewport/platform_viewport_headless.h"
#include "mojo/services/native_viewport/viewport_surface.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"
#include "ui/events/event.h"

namespace mojo {
namespace {

bool IsRateLimitedEventType(ui::Event* event) {
  return event->type() == ui::ET_MOUSE_MOVED ||
         event->type() == ui::ET_MOUSE_DRAGGED ||
         event->type() == ui::ET_TOUCH_MOVED;
}

}  // namespace

NativeViewportImpl::NativeViewportImpl(ApplicationImpl* app, bool is_headless)
    : is_headless_(is_headless),
      widget_id_(0u),
      waiting_for_event_ack_(false),
      weak_factory_(this) {
  app->ConnectToService("mojo:mojo_surfaces_service", &surfaces_service_);
  // TODO(jamesr): Should be mojo_gpu_service
  app->ConnectToService("mojo:mojo_native_viewport_service", &gpu_service_);
}

NativeViewportImpl::~NativeViewportImpl() {
  // Destroy the NativeViewport early on as it may call us back during
  // destruction and we want to be in a known state.
  platform_viewport_.reset();
}

void NativeViewportImpl::Create(SizePtr bounds) {
  if (is_headless_)
    platform_viewport_ = PlatformViewportHeadless::Create(this);
  else
    platform_viewport_ = PlatformViewport::Create(this);
  gfx::Rect rect = gfx::Rect(bounds.To<gfx::Size>());
  platform_viewport_->Init(rect);
  OnBoundsChanged(rect);
}

void NativeViewportImpl::Show() {
  platform_viewport_->Show();
}

void NativeViewportImpl::Hide() {
  platform_viewport_->Hide();
}

void NativeViewportImpl::Close() {
  DCHECK(platform_viewport_);
  platform_viewport_->Close();
}

void NativeViewportImpl::SetBounds(SizePtr bounds) {
  platform_viewport_->SetBounds(gfx::Rect(bounds.To<gfx::Size>()));
}

void NativeViewportImpl::SubmittedFrame(SurfaceIdPtr child_surface_id) {
  if (child_surface_id_.is_null()) {
    // If this is the first indication that the client will use surfaces,
    // initialize that system.
    // TODO(jamesr): When everything is converted to surfaces initialize this
    // eagerly.
    viewport_surface_.reset(
        new ViewportSurface(surfaces_service_.get(),
                            gpu_service_.get(),
                            bounds_.size(),
                            child_surface_id.To<cc::SurfaceId>()));
    if (widget_id_)
      viewport_surface_->SetWidgetId(widget_id_);
  }
  child_surface_id_ = child_surface_id.To<cc::SurfaceId>();
  if (viewport_surface_)
    viewport_surface_->SetChildId(child_surface_id_);
}

void NativeViewportImpl::OnBoundsChanged(const gfx::Rect& bounds) {
  bounds_ = bounds;
  client()->OnBoundsChanged(Size::From(bounds.size()));
  if (viewport_surface_)
    viewport_surface_->SetSize(bounds.size());
}

void NativeViewportImpl::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_id_ = static_cast<uint64_t>(bit_cast<uintptr_t>(widget));
  // TODO(jamesr): Remove once everything is converted to surfaces.
  client()->OnCreated(widget_id_);
  if (viewport_surface_)
    viewport_surface_->SetWidgetId(widget_id_);
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
      Event::From(*ui_event),
      base::Bind(&NativeViewportImpl::AckEvent, weak_factory_.GetWeakPtr()));
  waiting_for_event_ack_ = true;
  return false;
}

void NativeViewportImpl::OnDestroyed() {
  client()->OnDestroyed();
}

void NativeViewportImpl::AckEvent() {
  waiting_for_event_ack_ = false;
}

}  // namespace mojo

