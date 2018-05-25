// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_detailed_view_delegate.h"

#include "ash/system/unified/unified_system_tray_controller.h"

namespace ash {

UnifiedDetailedViewDelegate::UnifiedDetailedViewDelegate(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

UnifiedDetailedViewDelegate::~UnifiedDetailedViewDelegate() = default;

void UnifiedDetailedViewDelegate::TransitionToMainView(bool restore_focus) {
  tray_controller_->TransitionToMainView();
}

void UnifiedDetailedViewDelegate::CloseBubble() {
  tray_controller_->CloseBubble();
}

}  // namespace ash
