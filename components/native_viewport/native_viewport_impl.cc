// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/native_viewport/native_viewport_impl.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "components/gles2/gpu_state.h"
#include "components/native_viewport/platform_viewport_headless.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/events/event.h"

namespace native_viewport {

NativeViewportImpl::NativeViewportImpl(
    bool is_headless,
    const scoped_refptr<gles2::GpuState>& gpu_state,
    mojo::InterfaceRequest<mojo::NativeViewport> request)
    : is_headless_(is_headless),
      context_provider_(new OnscreenContextProvider(gpu_state)),
      sent_metrics_(false),
      metrics_(mojo::ViewportMetrics::New()),
      binding_(this, request.Pass()),
      weak_factory_(this) {
  binding_.set_error_handler(this);
}

NativeViewportImpl::~NativeViewportImpl() {
  // Destroy before |platform_viewport_| because this will destroy
  // CommandBufferDriver objects that contain child windows. Otherwise if this
  // class destroys its window first, X errors will occur.
  context_provider_.reset();

  // Destroy the NativeViewport early on as it may call us back during
  // destruction and we want to be in a known state.
  platform_viewport_.reset();
}

void NativeViewportImpl::Create(mojo::SizePtr size,
                                const CreateCallback& callback) {
  create_callback_ = callback;
  metrics_->size = size.Clone();
  if (is_headless_)
    platform_viewport_ = PlatformViewportHeadless::Create(this);
  else
    platform_viewport_ = PlatformViewport::Create(this);
  platform_viewport_->Init(gfx::Rect(size.To<gfx::Size>()));
}

void NativeViewportImpl::RequestMetrics(
    const RequestMetricsCallback& callback) {
  if (!sent_metrics_) {
    callback.Run(metrics_.Clone());
    sent_metrics_ = true;
    return;
  }
  metrics_callback_ = callback;
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

void NativeViewportImpl::SetSize(mojo::SizePtr size) {
  platform_viewport_->SetBounds(gfx::Rect(size.To<gfx::Size>()));
}

void NativeViewportImpl::GetContextProvider(
    mojo::InterfaceRequest<mojo::ContextProvider> request) {
  context_provider_->Bind(request.Pass());
}

void NativeViewportImpl::SetEventDispatcher(
    mojo::NativeViewportEventDispatcherPtr dispatcher) {
  event_dispatcher_ = dispatcher.Pass();
}

void NativeViewportImpl::OnMetricsChanged(mojo::ViewportMetricsPtr metrics) {
  if (metrics_->Equals(*metrics))
    return;

  metrics_ = metrics.Pass();
  sent_metrics_ = false;

  if (!metrics_callback_.is_null()) {
    metrics_callback_.Run(metrics_.Clone());
    metrics_callback_.reset();
    sent_metrics_ = true;
  }
}

void NativeViewportImpl::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_pixel_ratio) {
  metrics_->device_pixel_ratio = device_pixel_ratio;
  context_provider_->SetAcceleratedWidget(widget);
  // TODO: The metrics here might not match the actual window size on android
  // where we don't know the actual size until the first OnMetricsChanged call.
  create_callback_.Run(metrics_.Clone());
  sent_metrics_ = true;
  create_callback_.reset();
}

void NativeViewportImpl::OnAcceleratedWidgetDestroyed() {
  context_provider_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
}

bool NativeViewportImpl::OnEvent(mojo::EventPtr event) {
  if (event.is_null() || !event_dispatcher_.get())
    return false;

  mojo::NativeViewportEventDispatcher::OnEventCallback callback;
  switch (event->action) {
    case mojo::EVENT_TYPE_POINTER_MOVE: {
      // TODO(sky): add logic to remember last event location and not send if
      // the same.
      if (pointers_waiting_on_ack_.count(event->pointer_data->pointer_id))
        return false;

      pointers_waiting_on_ack_.insert(event->pointer_data->pointer_id);
      callback =
          base::Bind(&NativeViewportImpl::AckEvent, weak_factory_.GetWeakPtr(),
                     event->pointer_data->pointer_id);
      break;
    }

    case mojo::EVENT_TYPE_POINTER_CANCEL:
      pointers_waiting_on_ack_.clear();
      break;

    case mojo::EVENT_TYPE_POINTER_UP:
      pointers_waiting_on_ack_.erase(event->pointer_data->pointer_id);
      break;

    default:
      break;
  }

  event_dispatcher_->OnEvent(event.Pass(), callback);
  return false;
}

void NativeViewportImpl::OnDestroyed() {
  delete this;
}

void NativeViewportImpl::OnConnectionError() {
  binding_.set_error_handler(nullptr);
  delete this;
}

void NativeViewportImpl::AckEvent(int32 pointer_id) {
  pointers_waiting_on_ack_.erase(pointer_id);
}

}  // namespace native_viewport
