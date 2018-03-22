// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pods_container_view.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/unified_system_info_view.h"
#include "ui/app_list/app_list_features.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

UnifiedSystemTrayView::UnifiedSystemTrayView(
    UnifiedSystemTrayController* controller)
    : controller_(controller),
      top_shortcuts_view_(new TopShortcutsView(controller_)),
      feature_pods_container_(new FeaturePodsContainerView()),
      sliders_container_(new views::View),
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

  sliders_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

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
  sliders_container_->AddChildView(slider_view);
}

void UnifiedSystemTrayView::SetExpanded(bool expanded) {
  top_shortcuts_view_->SetExpanded(expanded);
  feature_pods_container_->SetExpanded(expanded);
  sliders_container_->SetVisible(expanded);
  PreferredSizeChanged();
}

}  // namespace ash
