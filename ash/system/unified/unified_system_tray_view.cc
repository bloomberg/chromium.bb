// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pods_container_view.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/unified_message_center_view.h"
#include "ash/system/unified/unified_system_info_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ui/app_list/app_list_features.h"
#include "ui/message_center/message_center.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace ash {

namespace {

std::unique_ptr<views::Background> CreateUnifiedBackground() {
  return views::CreateBackgroundFromPainter(
      views::Painter::CreateSolidRoundRectPainter(
          app_list::features::IsBackgroundBlurEnabled()
              ? kUnifiedMenuBackgroundColorWithBlur
              : kUnifiedMenuBackgroundColor,
          kUnifiedTrayCornerRadius));
}

}  // namespace

UnifiedSlidersContainerView::UnifiedSlidersContainerView(
    bool initially_expanded)
    : expanded_amount_(initially_expanded ? 1.0 : 0.0) {
  SetVisible(initially_expanded);
}

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
    UnifiedSystemTrayController* controller,
    bool initially_expanded)
    : controller_(controller),
      message_center_view_(
          new UnifiedMessageCenterView(message_center::MessageCenter::Get())),
      top_shortcuts_view_(new TopShortcutsView(controller_)),
      feature_pods_container_(new FeaturePodsContainerView(initially_expanded)),
      sliders_container_(new UnifiedSlidersContainerView(initially_expanded)),
      system_info_view_(new UnifiedSystemInfoView()) {
  DCHECK(controller_);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      kUnifiedNotificationCenterSpacing));

  SetBackground(CreateUnifiedBackground());
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  AddChildView(message_center_view_);
  layout->SetFlexForView(message_center_view_, 1);

  auto* system_tray_container = new views::View;
  system_tray_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  system_tray_container->SetBackground(CreateUnifiedBackground());
  AddChildView(system_tray_container);

  system_tray_container->AddChildView(top_shortcuts_view_);
  system_tray_container->AddChildView(feature_pods_container_);
  system_tray_container->AddChildView(sliders_container_);
  system_tray_container->AddChildView(system_info_view_);
}

UnifiedSystemTrayView::~UnifiedSystemTrayView() = default;

void UnifiedSystemTrayView::SetMaxHeight(int max_height) {
  message_center_view_->SetMaxHeight(max_height);
}

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
  // It is possible that the ratio between |message_center_view_| and others
  // can change while the bubble size remain unchanged.
  Layout();
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
