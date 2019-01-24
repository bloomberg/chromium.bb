// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_TRAY_ACTION_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_TRAY_ACTION_VIEW_H_

#include "ash/public/interfaces/accessibility_controller_enums.mojom.h"
#include "ash/system/tray/actionable_view.h"
#include "base/macros.h"

namespace ash {
class AutoclickTrayActionListView;

// A single action view in the autoclick tray bubble.
class AutoclickTrayActionView : public ActionableView {
 public:
  AutoclickTrayActionView(AutoclickTrayActionListView* list_view,
                          mojom::AutoclickEventType event_type,
                          const base::string16& label,
                          bool selected);
  ~AutoclickTrayActionView() override = default;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  AutoclickTrayActionListView* list_view_;
  mojom::AutoclickEventType event_type_;
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickTrayActionView);
};

}  // namespace ash

#endif  // defined ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_TRAY_ACTION_VIEW_H_
