// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_
#define ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class UnifiedSystemTrayController;
class TopShortcutButton;

// Top shortcuts view shown on the top of UnifiedSystemTrayView.
class TopShortcutsView : public views::View, public views::ButtonListener {
 public:
  explicit TopShortcutsView(UnifiedSystemTrayController* controller);
  ~TopShortcutsView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  UnifiedSystemTrayController* controller_;

  // Owned by views hierarchy.
  TopShortcutButton* lock_button_ = nullptr;
  TopShortcutButton* settings_button_ = nullptr;
  TopShortcutButton* power_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TopShortcutsView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_TOP_SHORTCUTS_VIEW_H_
