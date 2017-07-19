// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller_client_child_frame.h"

#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/content_switches_internal.h"
#include "content/common/view_messages.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
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

void TouchSelectionControllerClientChildFrame::UpdateSelectionBoundsIfNeeded(
    const cc::Selection<gfx::SelectionBound>& selection,
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

  gfx::Point origin = rwhv_->GetViewOriginInRoot();
  gfx::Vector2dF offset_v(origin.x(), origin.y());
  start_edge_top += offset_v;
  start_edge_bottom += offset_v;
  end_edge_top += offset_v;
  end_edge_bottom += offset_v;

  cc::Selection<gfx::SelectionBound> transformed_selection(selection);
  transformed_selection.start.SetEdge(start_edge_top, start_edge_bottom);
  transformed_selection.end.SetEdge(end_edge_top, end_edge_bottom);

  if (transformed_selection.start != selection_start_ ||
      transformed_selection.end != selection_end_) {
    selection_start_ = transformed_selection.start;
    selection_end_ = transformed_selection.end;
    manager_->UpdateClientSelectionBounds(selection_start_, selection_end_,
                                          this, this);
  }
}

gfx::Point TouchSelectionControllerClientChildFrame::ConvertFromRoot(
    const gfx::PointF& point_f) const {
  gfx::Point origin = rwhv_->GetViewOriginInRoot();
  return gfx::ToRoundedPoint(
      gfx::PointF(point_f.x() - origin.x(), point_f.y() - origin.y()));
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
      RenderWidgetHostImpl::From(rwhv_->GetRenderWidgetHost())->delegate();
  if (host_delegate)
    host_delegate->MoveCaret(ConvertFromRoot(position));
}

void TouchSelectionControllerClientChildFrame::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  RenderWidgetHostDelegate* host_delegate =
      RenderWidgetHostImpl::From(rwhv_->GetRenderWidgetHost())->delegate();
  if (host_delegate)
    host_delegate->MoveRangeSelectionExtent(ConvertFromRoot(extent));
}

void TouchSelectionControllerClientChildFrame::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  RenderWidgetHostDelegate* host_delegate =
      RenderWidgetHostImpl::From(rwhv_->GetRenderWidgetHost())->delegate();
  if (host_delegate) {
    host_delegate->SelectRange(ConvertFromRoot(base), ConvertFromRoot(extent));
  }
}

void TouchSelectionControllerClientChildFrame::OnSelectionEvent(
    ui::SelectionEventType event) {
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
      RenderWidgetHostImpl::From(rwhv_->GetRenderWidgetHost())->delegate();
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
  gfx::Point origin = rwhv_->GetViewOriginInRoot();
  anchor_point.Offset(-origin.x(), -origin.y());
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(rwhv_->GetRenderWidgetHost());
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
