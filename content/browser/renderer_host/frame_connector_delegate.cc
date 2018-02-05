// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_connector_delegate.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/common/content_switches_internal.h"

namespace content {

void FrameConnectorDelegate::SetView(RenderWidgetHostViewChildFrame* view) {
  view_ = view;
}

RenderWidgetHostViewBase*
FrameConnectorDelegate::GetParentRenderWidgetHostView() {
  return nullptr;
}

RenderWidgetHostViewBase*
FrameConnectorDelegate::GetRootRenderWidgetHostView() {
  return nullptr;
}

void FrameConnectorDelegate::UpdateResizeParams(
    const gfx::Rect& screen_space_rect,
    const gfx::Size& local_frame_size,
    const ScreenInfo& screen_info,
    uint64_t sequence_number,
    const viz::SurfaceId& surface_id) {
  screen_info_ = screen_info;
  local_surface_id_ = surface_id.local_surface_id();

  SetScreenSpaceRect(screen_space_rect);
  SetLocalFrameSize(local_frame_size);

  if (!view_)
    return;
#if defined(USE_AURA)
  view_->SetFrameSinkId(surface_id.frame_sink_id());
#endif  // defined(USE_AURA)

  RenderWidgetHostImpl* render_widget_host = view_->GetRenderWidgetHostImpl();
  DCHECK(render_widget_host);

  if (render_widget_host->auto_resize_enabled()) {
    render_widget_host->DidAllocateLocalSurfaceIdForAutoResize(sequence_number);
    return;
  }

  render_widget_host->WasResized();
}

gfx::PointF FrameConnectorDelegate::TransformPointToRootCoordSpace(
    const gfx::PointF& point,
    const viz::SurfaceId& surface_id) {
  return gfx::PointF();
}

bool FrameConnectorDelegate::TransformPointToLocalCoordSpace(
    const gfx::PointF& point,
    const viz::SurfaceId& original_surface,
    const viz::SurfaceId& local_surface_id,
    gfx::PointF* transformed_point) {
  return false;
}

bool FrameConnectorDelegate::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    const viz::SurfaceId& local_surface_id,
    gfx::PointF* transformed_point) {
  return false;
}

bool FrameConnectorDelegate::HasFocus() {
  return false;
}

bool FrameConnectorDelegate::LockMouse() {
  return false;
}

bool FrameConnectorDelegate::IsInert() const {
  return false;
}

bool FrameConnectorDelegate::IsHidden() const {
  return false;
}

bool FrameConnectorDelegate::IsThrottled() const {
  return false;
}

bool FrameConnectorDelegate::IsSubtreeThrottled() const {
  return false;
}

void FrameConnectorDelegate::SetLocalFrameSize(
    const gfx::Size& local_frame_size) {
  if (use_zoom_for_device_scale_factor_) {
    local_frame_size_in_pixels_ = local_frame_size;
    local_frame_size_in_dip_ = gfx::ScaleToCeiledSize(
        local_frame_size, 1.f / screen_info_.device_scale_factor);
  } else {
    local_frame_size_in_dip_ = local_frame_size;
    local_frame_size_in_pixels_ = gfx::ScaleToCeiledSize(
        local_frame_size, screen_info_.device_scale_factor);
  }
}

void FrameConnectorDelegate::SetScreenSpaceRect(
    const gfx::Rect& screen_space_rect) {
  if (use_zoom_for_device_scale_factor_) {
    screen_space_rect_in_pixels_ = screen_space_rect;
    screen_space_rect_in_dip_ = gfx::ScaleToEnclosingRect(
        screen_space_rect, 1.f / screen_info_.device_scale_factor);
  } else {
    screen_space_rect_in_dip_ = screen_space_rect;
    screen_space_rect_in_pixels_ = gfx::ScaleToEnclosingRect(
        screen_space_rect, screen_info_.device_scale_factor);
  }
}

FrameConnectorDelegate::FrameConnectorDelegate(
    bool use_zoom_for_device_scale_factor)
    : use_zoom_for_device_scale_factor_(use_zoom_for_device_scale_factor) {}

}  // namespace content
