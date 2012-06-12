// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/launcher/background_animator.h"
#include "ash/system/tray/tray_views.h"
#include "ash/wm/shelf_auto_hide_behavior.h"

namespace ash {
namespace internal {

class TrayBackground;

// Base class for children of StatusAreaWidget: SystemTray, WebNotificationTray.
// This class handles setting and animating the background when the Launcher
// his shown/hidden. It also inherits from ActionableView so that the tray
// items can override PerformAction when clicked on.

class ASH_EXPORT TrayBackgroundView : public internal::ActionableView,
                                      public BackgroundAnimatorDelegate {
 public:
  TrayBackgroundView();
  virtual ~TrayBackgroundView();

  // Overridden from views::View.
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;

  // Overridden from internal::ActionableView.
  virtual bool PerformAction(const views::Event& event) OVERRIDE;

  // Overridden from internal::BackgroundAnimatorDelegate.
  virtual void UpdateBackground(int alpha) OVERRIDE;

  // Called whenever the shelf alignment changes.
  virtual void SetShelfAlignment(ShelfAlignment alignment);

  // Sets |contents| as a child and sets its background to |background_|.
  void SetContents(views::View* contents);

  // Sets whether the tray paints a background. Default is true, but is set to
  // false if a window overlaps the shelf.
  void SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type);

  ShelfAlignment shelf_alignment() const { return shelf_alignment_; }

 private:
  // Shelf alignment.
  ShelfAlignment shelf_alignment_;

  // Owned by the view passed to SetContents().
  internal::TrayBackground* background_;

  internal::BackgroundAnimator hide_background_animator_;
  internal::BackgroundAnimator hover_background_animator_;

  DISALLOW_COPY_AND_ASSIGN(TrayBackgroundView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
