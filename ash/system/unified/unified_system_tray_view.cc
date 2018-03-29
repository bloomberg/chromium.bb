// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pods_container_view.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/unified_system_info_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/app_list/app_list_features.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

UnifiedSlidersContainerView::UnifiedSlidersContainerView() = default;

UnifiedSlidersContainerView::~UnifiedSlidersContainerView() = default;

void UnifiedSlidersContainerView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  SetVisible(expanded_amount > 0.0);
  expanded_amount_ = expanded_amount;
  PreferredSizeChanged();
  UpdateOpacity();
}

void UnifiedSlidersContainerView::Layout() {
  int y = 0;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    int height = child->GetHeightForWidth(kTrayMenuWidth);
    child->SetBounds(0, y, kTrayMenuWidth, height);
    y += height;
  }
}

gfx::Size UnifiedSlidersContainerView::CalculatePreferredSize() const {
  int height = 0;
  for (int i = 0; i < child_count(); ++i)
    height += child_at(i)->GetHeightForWidth(kTrayMenuWidth);
  return gfx::Size(kTrayMenuWidth, height * expanded_amount_);
}

void UnifiedSlidersContainerView::UpdateOpacity() {
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    double opacity = 1.0;
    if (child->y() > height())
      opacity = 0.0;
    else if (child->bounds().bottom() < height())
      opacity = 1.0;
    else
      opacity = static_cast<double>(height() - child->y()) / child->height();
    child->layer()->SetOpacity(opacity);
  }
}

UnifiedSystemTrayView::UnifiedSystemTrayView(
    UnifiedSystemTrayController* controller)
    : controller_(controller),
      top_shortcuts_view_(new TopShortcutsView(controller_)),
      feature_pods_container_(new FeaturePodsContainerView()),
      sliders_container_(new UnifiedSlidersContainerView()),
      system_info_view_(new UnifiedSystemInfoView()) {
  DCHECK(controller_);

  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  SetBackground(
      views::CreateSolidBackground(app_list::features::IsBackgroundBlurEnabled()
                                       ? kUnifiedMenuBackgroundColorWithBlur
                                       : kUnifiedMenuBackgroundColor));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  AddChildView(top_shortcuts_view_);
  AddChildView(feature_pods_container_);
  AddChildView(sliders_container_);
  AddChildView(system_info_view_);
}

UnifiedSystemTrayView::~UnifiedSystemTrayView() = default;

void UnifiedSystemTrayView::AddFeaturePodButton(FeaturePodButton* button) {
  feature_pods_container_->AddChildView(button);
}

void UnifiedSystemTrayView::AddSliderView(views::View* slider_view) {
  slider_view->SetPaintToLayer();
  slider_view->layer()->SetFillsBoundsOpaquely(false);
  sliders_container_->AddChildView(slider_view);
}

void UnifiedSystemTrayView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  if (expanded_amount == 1.0 || expanded_amount == 0.0)
    top_shortcuts_view_->SetExpanded(expanded_amount == 1.0);
  feature_pods_container_->SetExpandedAmount(expanded_amount);
  sliders_container_->SetExpandedAmount(expanded_amount);
  PreferredSizeChanged();
}

void UnifiedSystemTrayView::OnGestureEvent(ui::GestureEvent* event) {
  gfx::Point screen_location = event->location();
  ConvertPointToScreen(this, &screen_location);

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      controller_->BeginDrag(screen_location);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      controller_->UpdateDrag(screen_location);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      controller_->EndDrag(screen_location);
      event->SetHandled();
      break;
    default:
      break;
  }
}

}  // namespace ash
