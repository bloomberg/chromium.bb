// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_NOTE_ACTION_LAUNCH_BUTTON_H_
#define ASH_LOGIN_UI_NOTE_ACTION_LAUNCH_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/shell.h"
#include "ash/tray_action/tray_action.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"

namespace ash {

namespace mojom {
enum class TrayActionState;
}

// View for lock sreen UI element for launching note taking action handler app.
// The element is an image button with a semi-transparent bubble background,
// which is expanded upon hovering/focusing the element.
// The bubble is a quarter of a circle with the center in top right corner of
// the view (in LTR layout).
// The button is only visible if the lock screen note taking action is available
// (the view observes the action availability using login data dispatcher, and
// updates itself accordingly).
class ASH_EXPORT NoteActionLaunchButton : public NonAccessibleView {
 public:
  // Used by tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(NoteActionLaunchButton* launch_button);
    ~TestApi();

    // Gets the foreground, action image button view.
    const views::View* ActionButtonView() const;

    // Gets the background view.
    const views::View* BackgroundView() const;

   private:
    NoteActionLaunchButton* launch_button_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  explicit NoteActionLaunchButton(
      mojom::TrayActionState initial_note_action_state);
  ~NoteActionLaunchButton() override;

  // Updates the bubble visibility depending on the note taking action state.
  void UpdateVisibility(mojom::TrayActionState action_state);

 private:
  class BackgroundView;
  class ActionButton;

  // The background bubble view.
  BackgroundView* background_ = nullptr;

  // The actionable image button view.
  ActionButton* action_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NoteActionLaunchButton);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_NOTE_ACTION_LAUNCH_BUTTON_H_
