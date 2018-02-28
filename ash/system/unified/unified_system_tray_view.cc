// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_view.h"

#include "ash/system/unified/top_shortcuts_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

UnifiedSystemTrayView::UnifiedSystemTrayView(
    UnifiedSystemTrayController* controller)
    : controller_(controller) {
  DCHECK(controller_);

  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  AddChildView(new TopShortcutsView(controller_));
}

UnifiedSystemTrayView::~UnifiedSystemTrayView() = default;

}  // namespace ash
