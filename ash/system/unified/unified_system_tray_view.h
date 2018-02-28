// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_VIEW_H_

#include "ui/views/view.h"

namespace ash {

class UnifiedSystemTrayController;

// View class of default view in UnifiedSystemTray.
class UnifiedSystemTrayView : public views::View {
 public:
  explicit UnifiedSystemTrayView(UnifiedSystemTrayController* controller);
  ~UnifiedSystemTrayView() override;

 private:
  UnifiedSystemTrayController* controller_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_VIEW_H_
