// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_view.h"

#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/ui/zoom/zoom_controller.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"

ZoomView::ZoomView(LocationBarView::Delegate* location_bar_delegate)
    : BubbleIconView(nullptr, 0),
      location_bar_delegate_(location_bar_delegate) {
  Update(NULL);
}

ZoomView::~ZoomView() {
}

void ZoomView::Update(ui_zoom::ZoomController* zoom_controller) {
  if (!zoom_controller || zoom_controller->IsAtDefaultZoom() ||
      location_bar_delegate_->GetToolbarModel()->input_in_progress()) {
    SetVisible(false);
    ZoomBubbleView::CloseBubble();
    return;
  }

  SetTooltipText(l10n_util::GetStringFUTF16Int(
      IDS_TOOLTIP_ZOOM, zoom_controller->GetZoomPercent()));
  int image_id = IDR_ZOOM_NORMAL;
  ui_zoom::ZoomController::RelativeZoom relative_zoom =
      zoom_controller->GetZoomRelativeToDefault();
  if (relative_zoom == ui_zoom::ZoomController::ZOOM_BELOW_DEFAULT_ZOOM)
    image_id = IDR_ZOOM_MINUS;
  else if (relative_zoom == ui_zoom::ZoomController::ZOOM_ABOVE_DEFAULT_ZOOM)
    image_id = IDR_ZOOM_PLUS;

  SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(image_id));
  SetVisible(true);
}

bool ZoomView::IsBubbleShowing() const {
  return ZoomBubbleView::IsShowing();
}

void ZoomView::OnExecuting(BubbleIconView::ExecuteSource source) {
  ZoomBubbleView::ShowBubble(location_bar_delegate_->GetWebContents(), false);
}

void ZoomView::GetAccessibleState(ui::AXViewState* state) {
  BubbleIconView::GetAccessibleState(state);
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_ZOOM);
}
