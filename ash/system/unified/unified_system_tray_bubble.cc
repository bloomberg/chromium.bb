// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_bubble.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"

namespace ash {

UnifiedSystemTrayBubble::UnifiedSystemTrayBubble(UnifiedSystemTray* tray)
    : controller_(std::make_unique<UnifiedSystemTrayController>()),
      tray_(tray) {
  views::TrayBubbleView::InitParams init_params;
  init_params.anchor_alignment = views::TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM;
  init_params.min_width = kTrayMenuWidth;
  init_params.max_width = kTrayMenuWidth;
  init_params.delegate = tray;
  init_params.parent_window = tray->GetBubbleWindowContainer();
  init_params.anchor_view =
      tray->shelf()->GetSystemTrayAnchor()->GetBubbleAnchor();

  auto* bubble_view = new views::TrayBubbleView(init_params);
  bubble_view->AddChildView(controller_->CreateView());
  bubble_view->set_anchor_view_insets(
      tray->shelf()->GetSystemTrayAnchor()->GetBubbleAnchorInsets());
  bubble_view->set_color(kUnifiedMenuBackgroundColor);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view);
  bubble_widget_->AddObserver(this);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view->InitializeAndShowBubble();

  bubble_widget_->widget_delegate()->set_can_activate(true);
  bubble_widget_->Activate();
}

UnifiedSystemTrayBubble::~UnifiedSystemTrayBubble() {
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
}

void UnifiedSystemTrayBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
  tray_->CloseBubble();
}

}  // namespace ash
