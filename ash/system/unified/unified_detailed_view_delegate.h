// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_DETAILED_VIEW_DELEGATE_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_DETAILED_VIEW_DELEGATE_H_

#include "ash/system/tray/detailed_view_delegate.h"
#include "base/macros.h"

namespace ash {

class UnifiedSystemTrayController;

// Default implementation of DetailedViewDelegate for UnifiedSystemTray.
class UnifiedDetailedViewDelegate : public DetailedViewDelegate {
 public:
  explicit UnifiedDetailedViewDelegate(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedDetailedViewDelegate() override;

  // DetailedViewDelegate:
  void TransitionToMainView(bool restore_focus) override;
  void CloseBubble() override;

 private:
  UnifiedSystemTrayController* const tray_controller_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedDetailedViewDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_DETAILED_VIEW_DELEGATE_H_
