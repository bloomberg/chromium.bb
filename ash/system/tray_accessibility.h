// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_ACCESSIBILITY_H_
#define ASH_SYSTEM_TRAY_ACCESSIBILITY_H_

#include "ash/shell_delegate.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_image_item.h"
#include "ash/system/tray/tray_views.h"
#include "base/gtest_prod_util.h"

namespace chromeos {
class TrayAccessibilityTest;
}

namespace views {
class Button;
class ImageView;
class View;
}

namespace ash {

class SystemTrayItem;

class ASH_EXPORT AccessibilityObserver {
 public:
  virtual ~AccessibilityObserver() {}

  // Notifies when accessibilty mode changes.
  virtual void OnAccessibilityModeChanged(
      AccessibilityNotificationVisibility notify) = 0;
};

namespace internal {

class HoverHighlightView;

namespace tray {

class AccessibilityPopupView;

class AccessibilityDetailedView : public TrayDetailsView,
                                  public ViewClickListener,
                                  public views::ButtonListener,
                                  public ShellObserver {
 public:
  explicit AccessibilityDetailedView(SystemTrayItem* owner,
                                     user::LoginStatus login);
  virtual ~AccessibilityDetailedView() {}

 private:
  // Add the accessibility feature list.
  void AppendAccessibilityList();

  // Add help entries.
  void AppendHelpEntries();

  HoverHighlightView* AddScrollListItem(const string16& text,
                                        gfx::Font::FontStyle style,
                                        bool checked);
  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE;
  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  views::View* spoken_feedback_view_;
  views::View* high_contrast_view_;
  views::View* screen_magnifier_view_;;
  views::View* help_view_;

  bool spoken_feedback_enabled_;
  bool high_contrast_enabled_;
  bool screen_magnifier_enabled_;
  user::LoginStatus login_;

  friend class chromeos::TrayAccessibilityTest;
  DISALLOW_COPY_AND_ASSIGN(AccessibilityDetailedView);
};

}  // namespace tray

class TrayAccessibility : public TrayImageItem,
                          public AccessibilityObserver {
 public:
  explicit TrayAccessibility(SystemTray* system_tray);
  virtual ~TrayAccessibility();

 private:
  void SetTrayIconVisible(bool visible);
  tray::AccessibilityDetailedView* CreateDetailedMenu();

  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;

  // Overridden from AccessibilityObserver.
  virtual void OnAccessibilityModeChanged(
      AccessibilityNotificationVisibility notify) OVERRIDE;

  views::View* default_;
  tray::AccessibilityPopupView* detailed_popup_;
  tray::AccessibilityDetailedView* detailed_menu_;

  bool request_popup_view_;
  bool tray_icon_visible_;
  user::LoginStatus login_;

  // Bitmap of values from AccessibilityState enum.
  uint32 previous_accessibility_state_;

  // A11y feature status on just entering the lock screen.
  bool show_a11y_menu_on_lock_screen_;

  friend class chromeos::TrayAccessibilityTest;
  DISALLOW_COPY_AND_ASSIGN(TrayAccessibility);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_ACCESSIBILITY_H_
