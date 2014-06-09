// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_OVERVIEW_OVERVIEW_BUTTON_TRAY_H_
#define ASH_SYSTEM_OVERVIEW_OVERVIEW_BUTTON_TRAY_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_background_view.h"

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
                                      public ShellObserver {
 public:
  explicit OverviewButtonTray(StatusAreaWidget* status_area_widget);
  virtual ~OverviewButtonTray();

  // Updates the tray's visibility based on the LoginStatus and the current
  // state of MaximizeMode
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status);

  // ActionableView:
  virtual bool PerformAction(const ui::Event& event) OVERRIDE;

  // ShellObserver:
  virtual void OnMaximizeModeStarted() OVERRIDE;
  virtual void OnMaximizeModeEnded() OVERRIDE;

  // TrayBackgroundView:
  virtual bool ClickedOutsideBubble() OVERRIDE;
  virtual base::string16 GetAccessibleNameForTray() OVERRIDE;
  virtual void HideBubbleWithView(
      const views::TrayBubbleView* bubble_view) OVERRIDE;
  virtual void SetShelfAlignment(ShelfAlignment alignment) OVERRIDE;

 private:
  friend class OverviewButtonTrayTest;

  // Creates a new border for the icon. The padding is determined based on the
  // alignment of the shelf.
  void SetIconBorderForShelfAlignment();

  // Sets the icon to visible if maximize mode is enabled and
  // WindowSelectorController::CanSelect.
  void UpdateIconVisibility();

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  DISALLOW_COPY_AND_ASSIGN(OverviewButtonTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_OVERVIEW_OVERVIEW_BUTTON_TRAY_H_
