// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller_client_child_frame.h"

#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/common/view_messages.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/strings/grit/ui_strings.h"

namespace content {

TouchSelectionControllerClientChildFrame::
    TouchSelectionControllerClientChildFrame(
        RenderWidgetHostViewChildFrame* rwhv,
        TouchSelectionControllerClientManager* manager)
    : rwhv_(rwhv), manager_(manager) {
  DCHECK(rwhv);
  DCHECK(manager_);
}

TouchSelectionControllerClientChildFrame::
    ~TouchSelectionControllerClientChildFrame() {
  // If the manager doesn't outlive us, our owning view sill detach us.
  manager_->InvalidateClient(this);
}

void TouchSelectionControllerClientChildFrame::DidStopFlinging() {
  manager_->DidStopFlinging();
}

void TouchSelectionControllerClientChildFrame::UpdateSelectionBoundsIfNeeded(
    const viz::Selection<gfx::SelectionBound>& selection,
    float device_scale_factor) {
  gfx::PointF start_edge_top = selection.start.edge_top();
  gfx::PointF start_edge_bottom = selection.start.edge_bottom();
  gfx::PointF end_edge_top = selection.end.edge_top();
  gfx::PointF end_edge_bottom = selection.end.edge_bottom();

  if (IsUseZoomForDSFEnabled()) {
    float viewportToDIPScale = 1.0f / device_scale_factor;

    start_edge_top.Scale(viewportToDIPScale);
    start_edge_bottom.Scale(viewportToDIPScale);
    end_edge_top.Scale(viewportToDIPScale);
    end_edge_bottom.Scale(viewportToDIPScale);
  }

  gfx::PointF origin = rwhv_->TransformPointToRootCoordSpaceF(gfx::PointF());
  start_edge_top = rwhv_->TransformPointToRootCoordSpaceF(start_edge_top);
  start_edge_bottom = rwhv_->TransformPointToRootCoordSpaceF(start_edge_bottom);
  end_edge_top = rwhv_->TransformPointToRootCoordSpaceF(end_edge_top);
  end_edge_bottom = rwhv_->TransformPointToRootCoordSpaceF(end_edge_bottom);

  viz::Selection<gfx::SelectionBound> transformed_selection(selection);
  transformed_selection.start.SetEdge(start_edge_top, start_edge_bottom);
  transformed_selection.end.SetEdge(end_edge_top, end_edge_bottom);

  if (transformed_selection.start != selection_start_ ||
      transformed_selection.end != selection_end_) {
    selection_start_ = transformed_selection.start;
    selection_end_ = transformed_selection.end;
    view_origin_at_last_update_ = origin;
    manager_->UpdateClientSelectionBounds(selection_start_, selection_end_,
                                          this, this);
  }
}

// Since an active touch selection in a child frame can have its screen position
// changed by a scroll in a containing frame (and thus without the child frame
// sending a new compositor frame), we must manually recompute the screen
// position if requested to do so and it has changed.
void TouchSelectionControllerClientChildFrame::DidScroll() {
  gfx::PointF origin = rwhv_->TransformPointToRootCoordSpaceF(gfx::PointF());
  if (origin != view_origin_at_last_update_) {
    gfx::Vector2dF delta = origin - view_origin_at_last_update_;
    selection_start_.SetEdge(selection_start_.edge_top() + delta,
                             selection_start_.edge_bottom() + delta);
    selection_end_.SetEdge(selection_end_.edge_top() + delta,
                           selection_end_.edge_bottom() + delta);
    view_origin_at_last_update_ = origin;
    manager_->UpdateClientSelectionBounds(selection_start_, selection_end_,
                                          this, this);
  }
}

gfx::Point TouchSelectionControllerClientChildFrame::ConvertFromRoot(
    const gfx::PointF& point_f) const {
  gfx::PointF transformed_point(point_f);
  RenderWidgetHostViewBase* root_view = rwhv_->GetRootRenderWidgetHostView();
  if (root_view) {
    root_view->TransformPointToCoordSpaceForView(point_f, rwhv_,
                                                 &transformed_point);
  }

  return gfx::ToRoundedPoint(transformed_point);
}

bool TouchSelectionControllerClientChildFrame::SupportsAnimation() const {
  NOTREACHED();
  return false;
}

void TouchSelectionControllerClientChildFrame::SetNeedsAnimate() {
  NOTREACHED();
}

void TouchSelectionControllerClientChildFrame::MoveCaret(
    const gfx::PointF& position) {
  RenderWidgetHostDelegate* host_delegate =
      rwhv_->GetRenderWidgetHostImpl()->delegate();
  if (host_delegate)
    host_delegate->MoveCaret(ConvertFromRoot(position));
}

void TouchSelectionControllerClientChildFrame::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  RenderWidgetHostDelegate* host_delegate =
      rwhv_->GetRenderWidgetHostImpl()->delegate();
  if (host_delegate)
    host_delegate->MoveRangeSelectionExtent(ConvertFromRoot(extent));
}

void TouchSelectionControllerClientChildFrame::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  RenderWidgetHostDelegate* host_delegate =
      rwhv_->GetRenderWidgetHostImpl()->delegate();
  if (host_delegate) {
    host_delegate->SelectRange(ConvertFromRoot(base), ConvertFromRoot(extent));
  }
}

void TouchSelectionControllerClientChildFrame::OnSelectionEvent(
    ui::SelectionEventType event) {
  NOTREACHED();
}

void TouchSelectionControllerClientChildFrame::OnDragUpdate(
    const gfx::PointF& position) {
  NOTREACHED();
}

std::unique_ptr<ui::TouchHandleDrawable>
TouchSelectionControllerClientChildFrame::CreateDrawable() {
  NOTREACHED();
  return std::unique_ptr<ui::TouchHandleDrawable>();
}

bool TouchSelectionControllerClientChildFrame::IsCommandIdEnabled(
    int command_id) const {
  bool editable = rwhv_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
  bool readable = rwhv_->GetTextInputType() != ui::TEXT_INPUT_TYPE_PASSWORD;

  gfx::Range selection_range;
  bool has_selection =
      rwhv_->GetSelectionRange(&selection_range) && !selection_range.is_empty();
  switch (command_id) {
    case IDS_APP_CUT:
      return editable && readable && has_selection;
    case IDS_APP_COPY:
      return readable && has_selection;
    case IDS_APP_PASTE: {
      base::string16 result;
      ui::Clipboard::GetForCurrentThread()->ReadText(
          ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
      return editable && !result.empty();
    }
    default:
      return false;
  }
}

void TouchSelectionControllerClientChildFrame::ExecuteCommand(int command_id,
                                                              int event_flags) {
  manager_->GetTouchSelectionController()
      ->HideAndDisallowShowingAutomatically();
  RenderWidgetHostDelegate* host_delegate =
      rwhv_->GetRenderWidgetHostImpl()->delegate();
  if (!host_delegate)
    return;

  switch (command_id) {
    case IDS_APP_CUT:
      host_delegate->Cut();
      break;
    case IDS_APP_COPY:
      host_delegate->Copy();
      break;
    case IDS_APP_PASTE:
      host_delegate->Paste();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void TouchSelectionControllerClientChildFrame::RunContextMenu() {
  gfx::RectF anchor_rect =
      manager_->GetTouchSelectionController()->GetRectBetweenBounds();
  gfx::PointF anchor_point =
      gfx::PointF(anchor_rect.CenterPoint().x(), anchor_rect.y());
  gfx::PointF origin = rwhv_->TransformPointToRootCoordSpaceF(gfx::PointF());
  anchor_point.Offset(-origin.x(), -origin.y());
  RenderWidgetHostImpl* host = rwhv_->GetRenderWidgetHostImpl();
  host->Send(new ViewMsg_ShowContextMenu(host->GetRoutingID(),
                                         ui::MENU_SOURCE_TOUCH_EDIT_MENU,
                                         gfx::ToRoundedPoint(anchor_point)));

  // Hide selection handles after getting rect-between-bounds from touch
  // selection controller; otherwise, rect would be empty and the above
  // calculations would be invalid.
  manager_->GetTouchSelectionController()
      ->HideAndDisallowShowingAutomatically();
}

}  // namespace content
