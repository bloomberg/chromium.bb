// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/unified_accessibility_detailed_view_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/tray_accessibility.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

UnifiedAccessibilityDetailedViewController::
    UnifiedAccessibilityDetailedViewController(
        UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  Shell::Get()->accessibility_controller()->AddObserver(this);
}

UnifiedAccessibilityDetailedViewController::
    ~UnifiedAccessibilityDetailedViewController() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

views::View* UnifiedAccessibilityDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new tray::AccessibilityDetailedView(detailed_view_delegate_.get());
  return view_;
}

base::string16 UnifiedAccessibilityDetailedViewController::GetAccessibleName()
    const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_A11Y_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void UnifiedAccessibilityDetailedViewController::
    OnAccessibilityStatusChanged() {
  view_->OnAccessibilityStatusChanged();
}

}  // namespace ash
