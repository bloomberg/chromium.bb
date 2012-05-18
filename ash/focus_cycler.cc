// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/wm/window_cycle_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/views/focus/focus_search.h"
#include "ui/aura/window.h"
#include "ui/aura/client/activation_client.h"
#include "ui/views/accessible_pane_view.h"

namespace ash {

namespace internal {

FocusCycler::FocusCycler() : widget_activating_(NULL) {
}

FocusCycler::~FocusCycler() {
}

void FocusCycler::AddWidget(views::Widget* widget) {
  widgets_.push_back(widget);

  widget->GetFocusManager()->RegisterAccelerator(
      ui::Accelerator(ui::VKEY_F2, ui::EF_CONTROL_DOWN),
      ui::AcceleratorManager::kNormalPriority,
      this);
  widget->GetFocusManager()->RegisterAccelerator(
      ui::Accelerator(ui::VKEY_F1, ui::EF_CONTROL_DOWN),
      ui::AcceleratorManager::kNormalPriority,
      this);
}

void FocusCycler::RotateFocus(Direction direction) {
  int index = 0;
  int count = static_cast<int>(widgets_.size());
  int browser_index = count;

  for (; index < count; ++index) {
    if (widgets_[index]->IsActive())
      break;
  }

  int start_index = index;

  count = count + 1;

  for (;;) {
    if (direction == FORWARD)
      index = (index + 1) % count;
    else
      index = ((index - 1) + count) % count;

    // Ensure that we don't loop more than once.
    if (index == start_index)
      break;

    if (index == browser_index) {
      // Activate the first window.
      WindowCycleController::Direction window_direction =
          direction == FORWARD ? WindowCycleController::FORWARD :
                                 WindowCycleController::BACKWARD;
      ash::Shell::GetInstance()->window_cycle_controller()->HandleCycleWindow(
          window_direction, false);
      break;
    } else {
      if (FocusWidget(widgets_[index]))
        break;
    }
  }
}

bool FocusCycler::FocusWidget(views::Widget* widget) {
  views::AccessiblePaneView* view =
      static_cast<views::AccessiblePaneView*>(widget->GetContentsView());
  if (view->SetPaneFocusAndFocusDefault()) {
    widget_activating_ = widget;
    widget->Activate();
    widget_activating_ = NULL;
    if (widget->IsActive())
      return true;
  }
  return false;
}

bool FocusCycler::AcceleratorPressed(const ui::Accelerator& accelerator) {
  switch (accelerator.key_code()) {
    case ui::VKEY_F1:
      RotateFocus(BACKWARD);
      return true;
    case ui::VKEY_F2:
      RotateFocus(FORWARD);
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool FocusCycler::CanHandleAccelerators() const {
  return true;
}

}  // namespace internal

}  // namespace ash
