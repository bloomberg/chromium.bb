// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_ACCESSIBILITY_H_
#define ASH_SYSTEM_TRAY_ACCESSIBILITY_H_

#include <stdint.h>

#include "ash/accessibility_delegate.h"
#include "ash/shell_observer.h"
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

namespace gfx {
struct VectorIcon;
}

namespace views {
class Button;
class CustomButton;
class Label;
class View;
}

namespace ash {
class HoverHighlightView;
class SystemTrayItem;

namespace tray {

// A view for closable notification views, laid out like:
//  -------------------
// | icon  contents  x |
//  ----------------v--
// The close button will call OnClose() when clicked.
class AccessibilityPopupView : public views::View {
 public:
  explicit AccessibilityPopupView(uint32_t enabled_state_bits);

  const views::Label* label_for_test() const { return label_; }

  void Init();

 private:
  views::Label* CreateLabel(uint32_t enabled_state_bits);

  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityPopupView);
};

// Create the detailed view of accessibility tray.
class AccessibilityDetailedView : public TrayDetailsView,
                                  public ShellObserver {
 public:
  AccessibilityDetailedView(SystemTrayItem* owner, LoginStatus login);
  ~AccessibilityDetailedView() override {}

 private:
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

  // Helper function to create entries in the detailed accessibility view.
  HoverHighlightView* AddScrollListItem(const base::string16& text,
                                        bool checked,
                                        const gfx::VectorIcon& icon);
  HoverHighlightView* AddScrollListItemWithoutIcon(const base::string16& text,
                                                   bool checked);

  void AddSubHeader(const base::string16& header_text);

  views::View* spoken_feedback_view_ = nullptr;
  views::View* high_contrast_view_ = nullptr;
  views::View* screen_magnifier_view_ = nullptr;
  views::View* large_cursor_view_ = nullptr;
  views::CustomButton* help_view_ = nullptr;
  views::CustomButton* settings_view_ = nullptr;
  views::View* autoclick_view_ = nullptr;
  views::View* virtual_keyboard_view_ = nullptr;
  views::View* mono_audio_view_ = nullptr;
  views::View* caret_highlight_view_ = nullptr;
  views::View* highlight_mouse_cursor_view_ = nullptr;
  views::View* highlight_keyboard_focus_view_ = nullptr;
  views::View* sticky_keys_view_ = nullptr;
  views::View* tap_dragging_view_ = nullptr;

  bool spoken_feedback_enabled_ = false;
  bool high_contrast_enabled_ = false;
  bool screen_magnifier_enabled_ = false;
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

  friend class chromeos::TrayAccessibilityTest;
  DISALLOW_COPY_AND_ASSIGN(AccessibilityDetailedView);
};

}  // namespace tray

class TrayAccessibility : public TrayImageItem, public AccessibilityObserver {
 public:
  explicit TrayAccessibility(SystemTray* system_tray);
  ~TrayAccessibility() override;

 private:
  void SetTrayIconVisible(bool visible);
  tray::AccessibilityDetailedView* CreateDetailedMenu();

  // Overridden from TrayImageItem.
  bool GetInitialVisibility() override;
  views::View* CreateDefaultView(LoginStatus status) override;
  views::View* CreateDetailedView(LoginStatus status) override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(LoginStatus status) override;

  // Overridden from AccessibilityObserver.
  void OnAccessibilityModeChanged(
      AccessibilityNotificationVisibility notify) override;

  views::View* default_;
  tray::AccessibilityPopupView* detailed_popup_;
  tray::AccessibilityDetailedView* detailed_menu_;

  // Bitmap of fvalues from AccessibilityState.  Can contain any or
  // both of A11Y_SPOKEN_FEEDBACK A11Y_BRAILLE_DISPLAY_CONNECTED.
  uint32_t request_popup_view_state_;

  bool tray_icon_visible_;
  LoginStatus login_;

  // Bitmap of values from AccessibilityState enum.
  uint32_t previous_accessibility_state_;

  // A11y feature status on just entering the lock screen.
  bool show_a11y_menu_on_lock_screen_;

  friend class chromeos::TrayAccessibilityTest;
  DISALLOW_COPY_AND_ASSIGN(TrayAccessibility);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_ACCESSIBILITY_H_
