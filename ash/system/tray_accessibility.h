// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_ACCESSIBILITY_H_
#define ASH_SYSTEM_TRAY_ACCESSIBILITY_H_

#include <stdint.h>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/system/accessibility_observer.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_image_item.h"
#include "base/macros.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace chromeos {
class TrayAccessibilityTest;
}

namespace views {
class Button;
class Button;
class View;
}

namespace ash {
class HoverHighlightView;
class SystemTrayItem;

namespace tray {

// Create the detailed view of accessibility tray.
class AccessibilityDetailedView : public TrayDetailsView {
 public:
  explicit AccessibilityDetailedView(SystemTrayItem* owner);
  ~AccessibilityDetailedView() override {}

  void OnAccessibilityStatusChanged();

 private:
  friend class chromeos::TrayAccessibilityTest;

  // TrayDetailsView:
  void HandleViewClicked(views::View* view) override;
  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override;
  void CreateExtraTitleRowButtons() override;

  // Launches the WebUI settings in a browser and closes the system menu.
  void ShowSettings();

  // Launches the a11y help article in a browser and closes the system menu.
  void ShowHelp();

  // Add the accessibility feature list.
  void AppendAccessibilityList();

  HoverHighlightView* spoken_feedback_view_ = nullptr;
  HoverHighlightView* select_to_speak_view_ = nullptr;
  HoverHighlightView* high_contrast_view_ = nullptr;
  HoverHighlightView* screen_magnifier_view_ = nullptr;
  HoverHighlightView* docked_magnifier_view_ = nullptr;
  HoverHighlightView* large_cursor_view_ = nullptr;
  HoverHighlightView* autoclick_view_ = nullptr;
  HoverHighlightView* virtual_keyboard_view_ = nullptr;
  HoverHighlightView* mono_audio_view_ = nullptr;
  HoverHighlightView* caret_highlight_view_ = nullptr;
  HoverHighlightView* highlight_mouse_cursor_view_ = nullptr;
  HoverHighlightView* highlight_keyboard_focus_view_ = nullptr;
  HoverHighlightView* sticky_keys_view_ = nullptr;
  HoverHighlightView* tap_dragging_view_ = nullptr;
  views::Button* help_view_ = nullptr;
  views::Button* settings_view_ = nullptr;

  // These exist for tests. The canonical state is stored in prefs.
  bool spoken_feedback_enabled_ = false;
  bool select_to_speak_enabled_ = false;
  bool high_contrast_enabled_ = false;
  bool screen_magnifier_enabled_ = false;
  bool docked_magnifier_enabled_ = false;
  bool large_cursor_enabled_ = false;
  bool autoclick_enabled_ = false;
  bool virtual_keyboard_enabled_ = false;
  bool mono_audio_enabled_ = false;
  bool caret_highlight_enabled_ = false;
  bool highlight_mouse_cursor_enabled_ = false;
  bool highlight_keyboard_focus_enabled_ = false;
  bool sticky_keys_enabled_ = false;
  bool tap_dragging_enabled_ = false;

  LoginStatus login_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityDetailedView);
};

}  // namespace tray

class TrayAccessibility : public TrayImageItem, public AccessibilityObserver {
 public:
  explicit TrayAccessibility(SystemTray* system_tray);
  ~TrayAccessibility() override;

 private:
  friend class chromeos::TrayAccessibilityTest;

  void SetTrayIconVisible(bool visible);
  tray::AccessibilityDetailedView* CreateDetailedMenu();

  // Overridden from TrayImageItem.
  bool GetInitialVisibility() override;
  views::View* CreateDefaultView(LoginStatus status) override;
  views::View* CreateDetailedView(LoginStatus status) override;
  void OnDefaultViewDestroyed() override;
  void OnDetailedViewDestroyed() override;
  void UpdateAfterLoginStatusChange(LoginStatus status) override;

  // Overridden from AccessibilityObserver.
  void OnAccessibilityStatusChanged(
      AccessibilityNotificationVisibility notify) override;

  views::View* default_;
  tray::AccessibilityDetailedView* detailed_menu_;

  bool tray_icon_visible_;
  LoginStatus login_;

  // Bitmap of values from AccessibilityState enum.
  uint32_t previous_accessibility_state_;

  // A11y feature status on just entering the lock screen.
  bool show_a11y_menu_on_lock_screen_;

  DISALLOW_COPY_AND_ASSIGN(TrayAccessibility);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_ACCESSIBILITY_H_
