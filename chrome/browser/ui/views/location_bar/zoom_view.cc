// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_view.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/toolbar/toolbar_model.h"
#include "components/ui/zoom/zoom_controller.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icons_public.h"

ZoomView::ZoomView(LocationBarView::Delegate* location_bar_delegate)
    : BubbleIconView(nullptr, 0),
      location_bar_delegate_(location_bar_delegate),
      image_id_(gfx::VectorIconId::VECTOR_ICON_NONE) {
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

  // The icon is hidden when the zoom level is default.
  image_id_ = zoom_controller->GetZoomRelativeToDefault() ==
                      ui_zoom::ZoomController::ZOOM_BELOW_DEFAULT_ZOOM
                  ? gfx::VectorIconId::ZOOM_MINUS
                  : gfx::VectorIconId::ZOOM_PLUS;
  if (GetNativeTheme())
    UpdateIcon();

  SetVisible(true);
}

void ZoomView::OnExecuting(BubbleIconView::ExecuteSource source) {
  ZoomBubbleView::ShowBubble(location_bar_delegate_->GetWebContents(),
                             ZoomBubbleView::USER_GESTURE);
}

void ZoomView::GetAccessibleState(ui::AXViewState* state) {
  BubbleIconView::GetAccessibleState(state);
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_ZOOM);
}

views::BubbleDelegateView* ZoomView::GetBubble() const {
  return ZoomBubbleView::GetZoomBubble();
}

gfx::VectorIconId ZoomView::GetVectorIcon() const {
  return image_id_;
}
