// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host_client_aura.h"

#include "content/browser/renderer_host/compositor_resize_lock_aura.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/common/view_messages.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"

namespace content {
namespace {

// When accelerated compositing is enabled and a widget resize is pending,
// we delay further resizes of the UI. The following constant is the maximum
// length of time that we should delay further UI resizes while waiting for a
// resized frame from a renderer.
const int kResizeLockTimeoutMs = 67;

}  // namespace

DelegatedFrameHostClientAura::DelegatedFrameHostClientAura(
    RenderWidgetHostViewAura* render_widget_host_view)
    : render_widget_host_view_(render_widget_host_view) {}

DelegatedFrameHostClientAura::~DelegatedFrameHostClientAura() {}

ui::Layer* DelegatedFrameHostClientAura::DelegatedFrameHostGetLayer() const {
  return render_widget_host_view_->window_->layer();
}

bool DelegatedFrameHostClientAura::DelegatedFrameHostIsVisible() const {
  return !render_widget_host_view_->host_->is_hidden();
}

SkColor DelegatedFrameHostClientAura::DelegatedFrameHostGetGutterColor(
    SkColor color) const {
  // When making an element on the page fullscreen the element's background
  // may not match the page's, so use black as the gutter color to avoid
  // flashes of brighter colors during the transition.
  if (render_widget_host_view_->host_->delegate() &&
      render_widget_host_view_->host_->delegate()
          ->IsFullscreenForCurrentTab()) {
    return SK_ColorBLACK;
  }
  return color;
}

gfx::Size DelegatedFrameHostClientAura::DelegatedFrameHostDesiredSizeInDIP()
    const {
  return render_widget_host_view_->window_->bounds().size();
}

bool DelegatedFrameHostClientAura::DelegatedFrameCanCreateResizeLock() const {
#if !defined(OS_CHROMEOS)
  // On Windows and Linux, holding pointer moves will not help throttling
  // resizes.
  // TODO(piman): on Windows we need to block (nested message loop?) the
  // WM_SIZE event. On Linux we need to throttle at the WM level using
  // _NET_WM_SYNC_REQUEST.
  return false;
#else
  if (!render_widget_host_view_->host_->renderer_initialized() ||
      render_widget_host_view_->host_->auto_resize_enabled()) {
    return false;
  }
  return true;
#endif
}

std::unique_ptr<ResizeLock>
DelegatedFrameHostClientAura::DelegatedFrameHostCreateResizeLock(
    bool defer_compositor_lock) {
  gfx::Size desired_size = render_widget_host_view_->window_->bounds().size();
  return std::unique_ptr<ResizeLock>(new CompositorResizeLock(
      render_widget_host_view_->window_->GetHost(), desired_size,
      defer_compositor_lock,
      base::TimeDelta::FromMilliseconds(kResizeLockTimeoutMs)));
}

void DelegatedFrameHostClientAura::DelegatedFrameHostResizeLockWasReleased() {
  render_widget_host_view_->host_->WasResized();
}

void DelegatedFrameHostClientAura::
    DelegatedFrameHostSendReclaimCompositorResources(
        int compositor_frame_sink_id,
        bool is_swap_ack,
        const cc::ReturnedResourceArray& resources) {
  render_widget_host_view_->host_->Send(new ViewMsg_ReclaimCompositorResources(
      render_widget_host_view_->host_->GetRoutingID(), compositor_frame_sink_id,
      is_swap_ack, resources));
}

void DelegatedFrameHostClientAura::SetBeginFrameSource(
    cc::BeginFrameSource* source) {
  if (render_widget_host_view_->begin_frame_source_ &&
      render_widget_host_view_->added_frame_observer_) {
    render_widget_host_view_->begin_frame_source_->RemoveObserver(
        render_widget_host_view_);
    render_widget_host_view_->added_frame_observer_ = false;
  }
  render_widget_host_view_->begin_frame_source_ = source;
  render_widget_host_view_->UpdateNeedsBeginFramesInternal();
}

bool DelegatedFrameHostClientAura::IsAutoResizeEnabled() const {
  return render_widget_host_view_->host_->auto_resize_enabled();
}

}  // namespace content
