// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_OVERVIEW_OVERVIEW_BUTTON_TRAY_H_
#define ASH_SYSTEM_OVERVIEW_OVERVIEW_BUTTON_TRAY_H_

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "ui/events/event_constants.h"

namespace views {
class ImageView;
}

namespace ash {

// Status area tray for showing a toggle for Overview Mode. Overview Mode
// is equivalent to WindowSelectorController being in selection mode.
// This hosts a ShellObserver that listens for the activation of Maximize Mode
// This tray will only be visible while in this state. This tray does not
// provide any bubble view windows.
class ASH_EXPORT OverviewButtonTray : public TrayBackgroundView,
                                      public SessionObserver,
                                      public ShellObserver,
                                      public TabletModeObserver {
 public:
  explicit OverviewButtonTray(Shelf* shelf);
  ~OverviewButtonTray() override;

  // Updates the tray's visibility based on the LoginStatus and the current
  // state of TabletMode
  virtual void UpdateAfterLoginStatusChange(LoginStatus status);

  // views::Button:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ShellObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view) override;

 private:
  friend class OverviewButtonTrayTest;

  // Sets the icon to visible if tablet mode is enabled and
  // WindowSelectorController::CanSelect.
  void UpdateIconVisibility();

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  ScopedSessionObserver scoped_session_observer_;

  DISALLOW_COPY_AND_ASSIGN(OverviewButtonTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_OVERVIEW_OVERVIEW_BUTTON_TRAY_H_
