// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"

#include "ash/shell.h"
#include "ash/wm/window_cycle_controller.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/window.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool HasFocusableWindow() {
  return !WindowCycleController::BuildWindowList(NULL, false).empty();
}

}  // namespace

namespace internal {

FocusCycler::FocusCycler() : widget_activating_(NULL) {
}

FocusCycler::~FocusCycler() {
}

void FocusCycler::AddWidget(views::Widget* widget) {
  widgets_.push_back(widget);
}

void FocusCycler::RotateFocus(Direction direction) {
  const bool has_window = HasFocusableWindow();
  int index = 0;
  int count = static_cast<int>(widgets_.size());
  int browser_index = has_window ? count : -1;

  for (; index < count; ++index) {
    if (widgets_[index]->IsActive())
      break;
  }

  int start_index = index;

  if (has_window)
    ++count;

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
  // Note: It is not necessary to set the focus directly to the pane since that
  // will be taken care of by the widget activation.
  widget_activating_ = widget;
  widget->Activate();
  widget_activating_ = NULL;
  return widget->IsActive();
}

}  // namespace internal

}  // namespace ash
