// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_OVERVIEW_TOOLBAR_H_
#define ATHENA_WM_OVERVIEW_TOOLBAR_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace gfx {
class Transform;
}

namespace ui {
class GestureEvent;
}

namespace athena {

class ActionButton;

// Responsible for showing action-buttons at the right edge of the screen during
// overview mode.
class OverviewToolbar {
 public:
  enum ActionType {
    ACTION_TYPE_NONE,
    ACTION_TYPE_CLOSE,
    ACTION_TYPE_SPLIT,
  };

  explicit OverviewToolbar(aura::Window* container);
  virtual ~OverviewToolbar();

  ActionType current_action() const { return current_action_; }

  // Returns the action the gesture-event is targeting.
  ActionType GetHighlightAction(const ui::GestureEvent& event) const;

  void SetHighlightAction(ActionType action);
  void ShowActionButtons();
  void HideActionButtons();

  void DisableAction(ActionType action);

 private:
  void ToggleActionButtonsVisibility();
  bool IsActionEnabled(ActionType action) const;
  bool IsEventOverButton(ActionButton* button,
                         const ui::GestureEvent& event) const;
  gfx::Transform ComputeTransformFor(ActionButton* button) const;
  void TransformButton(ActionButton* button);

  bool shown_;
  uint64_t disabled_action_bitfields_;
  scoped_ptr<ActionButton> close_;
  scoped_ptr<ActionButton> split_;
  ActionType current_action_;
  const gfx::Rect container_bounds_;

  DISALLOW_COPY_AND_ASSIGN(OverviewToolbar);
};

}  // namespace athena

#endif  // ATHENA_WM_OVERVIEW_TOOLBAR_H_
