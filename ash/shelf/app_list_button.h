// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_APP_LIST_BUTTON_H_
#define ASH_SHELF_APP_LIST_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/voice_interaction_state.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/button/image_button.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash {
class InkDropButtonListener;
class Shelf;
class ShelfView;
class VoiceInteractionOverlay;

// Button used for the AppList icon on the shelf.
class ASH_EXPORT AppListButton : public views::ImageButton,
                                 public ShellObserver,
                                 public SessionObserver {
 public:
  AppListButton(InkDropButtonListener* listener,
                ShelfView* shelf_view,
                Shelf* shelf);
  ~AppListButton() override;

  void OnAppListShown();
  void OnAppListDismissed();

  bool is_showing_app_list() const { return is_showing_app_list_; }

  // Updates background and schedules a paint.
  void UpdateShelfItemBackground(SkColor color);

  // views::ImageButton:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Get the center point of the app list button circle used to draw its
  // background and ink drops.
  gfx::Point GetAppListButtonCenterPoint() const;

  // Get the center point of the app list button back arrow. Returns an empty
  // gfx::Point if the back arrow is not shown.
  gfx::Point GetBackButtonCenterPoint() const;

  // Called by ShelfView to notify the app list button that it has started or
  // finished a bounds animation.
  void OnBoundsAnimationStarted();
  void OnBoundsAnimationFinished();

 protected:
  // views::ImageButton:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  void NotifyClick(const ui::Event& event) override;
  bool ShouldEnterPushedState(const ui::Event& event) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // ShellObserver:
  void OnAppListVisibilityChanged(bool shown,
                                  aura::Window* root_window) override;
  void OnVoiceInteractionStatusChanged(
      ash::VoiceInteractionState state) override;
  void OnVoiceInteractionEnabled(bool enabled) override;
  void OnVoiceInteractionSetupCompleted() override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  void StartVoiceInteractionAnimation();

  // Helper function to determine whether and event at |location| should be
  // handled by the back button or the app list circle. Returns false if we are
  // not in tablet mode (there is no back button).
  bool IsBackEvent(const gfx::Point& location);

  // Generate and send a VKEY_BROWSER_BACK key event when the back button
  // portion is clicked or tapped.
  void GenerateAndSendBackEvent(const ui::LocatedEvent& original_event);

  // Whether the voice interaction style should be used.
  bool UseVoiceInteractionStyle();

  // Initialize the voice interaction overlay.
  void InitializeVoiceInteractionOverlay();

  // Whether the active user is the primary user.
  bool IsUserPrimary();

  // True if the app list is currently showing for this display.
  // This is useful because other IsApplistVisible functions aren't per-display.
  bool is_showing_app_list_;

  // Color used to paint the background.
  SkColor background_color_;

  InkDropButtonListener* listener_;
  ShelfView* shelf_view_;
  Shelf* shelf_;

  // Owned by the view hierarchy. Null if the voice interaction is not enabled.
  VoiceInteractionOverlay* voice_interaction_overlay_ = nullptr;
  std::unique_ptr<base::OneShotTimer> voice_interaction_animation_delay_timer_;
  std::unique_ptr<base::OneShotTimer>
      voice_interaction_animation_hide_delay_timer_;

  // Flag that gets set each time we receive a mouse or gesture event. It is
  // then used to render the ink drop in the right location.
  bool last_event_is_back_event_ = false;

  DISALLOW_COPY_AND_ASSIGN(AppListButton);
};

}  // namespace ash

#endif  // ASH_SHELF_APP_LIST_BUTTON_H_
