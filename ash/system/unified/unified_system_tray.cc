// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray.h"

#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"

namespace ash {

UnifiedSystemTray::UnifiedSystemTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  // On the first step, features in the status area button are still provided by
  // TrayViews in SystemTray.
  // TODO(tetsui): Remove SystemTray from StatusAreaWidget and provide these
  // features from UnifiedSystemTray.
  SetVisible(false);
}

UnifiedSystemTray::~UnifiedSystemTray() = default;

bool UnifiedSystemTray::IsBubbleShown() const {
  return !!bubble_;
}

bool UnifiedSystemTray::PerformAction(const ui::Event& event) {
  if (bubble_)
    CloseBubble();
  else
    ShowBubble(true /* show_by_click */);
  return true;
}

void UnifiedSystemTray::ShowBubble(bool show_by_click) {
  bubble_ = std::make_unique<UnifiedSystemTrayBubble>(this);
  // TODO(tetsui): Call its own SetIsActive. See the comment in the ctor.
  shelf()->GetStatusAreaWidget()->system_tray()->SetIsActive(true);
}

void UnifiedSystemTray::CloseBubble() {
  bubble_.reset();
  // TODO(tetsui): Call its own SetIsActive. See the comment in the ctor.
  shelf()->GetStatusAreaWidget()->system_tray()->SetIsActive(false);
}

base::string16 UnifiedSystemTray::GetAccessibleNameForTray() {
  // TODO(tetsui): Implement.
  return base::string16();
}

void UnifiedSystemTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {}

void UnifiedSystemTray::ClickedOutsideBubble() {}

}  // namespace ash
