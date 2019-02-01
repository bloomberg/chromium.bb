// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_APP_LIST_BUTTON_H_
#define ASH_SHELF_APP_LIST_BUTTON_H_

#include <memory>

#include "ash/app_list/app_list_controller_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/shelf/shelf_control_button.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace ash {

class AssistantOverlay;
class Shelf;
class ShelfView;

// Button used for the AppList icon on the shelf.
class ASH_EXPORT AppListButton : public ShelfControlButton,
                                 public AppListControllerObserver,
                                 public SessionObserver,
                                 public TabletModeObserver,
                                 public DefaultVoiceInteractionObserver {
 public:
  AppListButton(ShelfView* shelf_view, Shelf* shelf);
  ~AppListButton() override;

  void OnAppListShown();
  void OnAppListDismissed();

  bool is_showing_app_list() const { return is_showing_app_list_; }

  // views::Button:
  void OnGestureEvent(ui::GestureEvent* event) override;

 protected:
  // views::Button:
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

  // mojom::VoiceInteractionObserver:
  void OnVoiceInteractionStatusChanged(
      mojom::VoiceInteractionState state) override;
  void OnVoiceInteractionSettingsEnabled(bool enabled) override;
  void OnVoiceInteractionSetupCompleted(bool completed) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;

  void StartVoiceInteractionAnimation();

  // Whether the voice interaction style should be used.
  bool UseVoiceInteractionStyle();

  // Initialize the voice interaction overlay.
  void InitializeVoiceInteractionOverlay();

  // True if the app list is currently showing for this display.
  // This is useful because other app_list_visible functions aren't per-display.
  bool is_showing_app_list_ = false;

  Shelf* shelf_;

  // Owned by the view hierarchy. Null if the voice interaction is not enabled.
  AssistantOverlay* assistant_overlay_ = nullptr;
  std::unique_ptr<base::OneShotTimer> assistant_animation_delay_timer_;
  std::unique_ptr<base::OneShotTimer> assistant_animation_hide_delay_timer_;
  base::TimeTicks voice_interaction_start_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(AppListButton);
};

}  // namespace ash

#endif  // ASH_SHELF_APP_LIST_BUTTON_H_
