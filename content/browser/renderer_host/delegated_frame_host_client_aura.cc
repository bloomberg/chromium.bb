// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host_client_aura.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/common/view_messages.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"

namespace content {

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
  // TODO(piman): on Windows we need to block (nested run loop?) the
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

std::unique_ptr<CompositorResizeLock>
DelegatedFrameHostClientAura::DelegatedFrameHostCreateResizeLock() {
  // Pointer moves are released when the CompositorResizeLock ends.
  auto* host = render_widget_host_view_->window_->GetHost();
  host->dispatcher()->HoldPointerMoves();

  gfx::Size desired_size = render_widget_host_view_->window_->bounds().size();
  return base::MakeUnique<CompositorResizeLock>(this, desired_size);
}

viz::LocalSurfaceId DelegatedFrameHostClientAura::GetLocalSurfaceId() const {
  return render_widget_host_view_->GetLocalSurfaceId();
}

void DelegatedFrameHostClientAura::OnBeginFrame() {
  render_widget_host_view_->OnBeginFrame();
}

bool DelegatedFrameHostClientAura::IsAutoResizeEnabled() const {
  return render_widget_host_view_->host_->auto_resize_enabled();
}

std::unique_ptr<ui::CompositorLock>
DelegatedFrameHostClientAura::GetCompositorLock(
    ui::CompositorLockClient* client) {
  auto* window_host = render_widget_host_view_->window_->GetHost();
  return window_host->compositor()->GetCompositorLock(client);
}

void DelegatedFrameHostClientAura::CompositorResizeLockEnded() {
  auto* window_host = render_widget_host_view_->window_->GetHost();
  window_host->dispatcher()->ReleasePointerMoves();
  render_widget_host_view_->host_->WasResized();
}

}  // namespace content
