// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_accessibility.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "grit/ui_resources.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace internal {

TrayAccessibility::TrayAccessibility()
    : TrayImageItem(IDR_AURA_UBER_TRAY_ACCESSIBILITY) {
}

TrayAccessibility::~TrayAccessibility() {}

bool TrayAccessibility::ShouldDisplay() {
  return ash::Shell::GetInstance()->tray_delegate()->IsInAccessibilityMode();
}

void TrayAccessibility::OnAccessibilityModeChanged(bool enabled) {
  if (image_view())
    image_view()->SetVisible(enabled);
}

}  // namespace internal
}  // namespace ash
