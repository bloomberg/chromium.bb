// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_connector_delegate.h"

namespace content {

RenderWidgetHostViewBase*
FrameConnectorDelegate::GetParentRenderWidgetHostView() {
  return nullptr;
}

RenderWidgetHostViewBase*
FrameConnectorDelegate::GetRootRenderWidgetHostView() {
  return nullptr;
}

gfx::Rect FrameConnectorDelegate::ChildFrameRect() {
  return gfx::Rect();
}

gfx::Point FrameConnectorDelegate::TransformPointToRootCoordSpace(
    const gfx::Point& point,
    const viz::SurfaceId& surface_id) {
  return gfx::Point();
}

bool FrameConnectorDelegate::TransformPointToLocalCoordSpace(
    const gfx::Point& point,
    const viz::SurfaceId& original_surface,
    const viz::SurfaceId& local_surface_id,
    gfx::Point* transformed_point) {
  return false;
}

bool FrameConnectorDelegate::TransformPointToCoordSpaceForView(
    const gfx::Point& point,
    RenderWidgetHostViewBase* target_view,
    const viz::SurfaceId& local_surface_id,
    gfx::Point* transformed_point) {
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

}  // namespace content
