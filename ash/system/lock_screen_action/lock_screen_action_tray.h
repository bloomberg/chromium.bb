// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_TRAY_H_
#define ASH_SYSTEM_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_TRAY_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"

namespace gfx {
class ImageSkia;
}

namespace ash {

class SessionController;
class TrayAction;

// Tray item for surfacing entry point on lock screen UI for handling actions
// available through mojom::TrayAction interface. The item is
// shown as a system tray status area icon. Its visibility is determined by:
// * session state - the item is shown only when the session is locked
// * tray action state - the item is shown only when the action is available
//       or the action handler is being launched.
// The tray item action is performed by sending a request to handle the tray
// action to registered mojom::TrayActionClient.
//
// Currently, only one action is supported - new (lock screen) note. This class
// thus uses the assets associated with new note action.
class ASH_EXPORT LockScreenActionTray : public TrayBackgroundView,
                                        public SessionObserver,
                                        public TrayActionObserver {
 public:
  explicit LockScreenActionTray(WmShelf* wm_shelf);
  ~LockScreenActionTray() override;

  // TrayBackgroundView:
  void Initialize() override;
  bool PerformAction(const ui::Event& event) override;
  base::string16 GetAccessibleNameForTray() override;
  bool ShouldBlockShelfAutoHide() const;
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;

  const gfx::ImageSkia& GetImageForTesting() const;

 private:
  class NewNoteActionView;

  // Whether the tray item should be visible in its current state.
  bool IsStateVisible() const;

  // Updates the icon displayed in the tray for the note creation action.
  void UpdateNewNoteIcon();

  // The view for the tray item - it's a wrapper around a ImageView.
  NewNoteActionView* new_note_action_view_;

  mojom::TrayActionState new_note_state_ =
      mojom::TrayActionState::kNotAvailable;

  ScopedObserver<SessionController, SessionObserver> session_observer_;
  ScopedObserver<TrayAction, TrayActionObserver> tray_action_observer_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenActionTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_TRAY_H_
