// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_TRAY_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_TRAY_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/interfaces/accessibility_controller_enums.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// A button in the tray that opens a menu which lets users manage their
// autoclick settings.
class ASH_EXPORT AutoclickTray : public TrayBackgroundView,
                                 public AccessibilityObserver,
                                 public SessionObserver {
 public:
  explicit AutoclickTray(Shelf* shelf);
  ~AutoclickTray() override;

  // TrayBackgroundView:
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  bool PerformAction(const ui::Event& event) override;
  void CloseBubble() override;
  void ShowBubble(bool show_by_click) override;
  TrayBubbleView* GetBubbleView() override;

  // TrayBubbleView::Delegate:
  base::string16 GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // Whether the tray button or the bubble, if the bubble exists, contain
  // the given screen point.
  bool ContainsPointInScreen(const gfx::Point& point);

  // Called when the user wants to open the autoclick section of the chrome
  // settings. Used when the bubble menu's settings button is tapped.
  void OnSettingsPressed();

  // Called when the user wants to change the autoclick event type. Used when
  // an event type in the bubble menu is tapped.
  void OnEventTypePressed(mojom::AutoclickEventType type);

 private:
  friend class AutoclickTrayTest;
  friend class AutoclickTest;

  // Updates the icons color depending on if the user is logged-in or not.
  void UpdateIconsForSession();

  // Updates visibility when autoclick is enabled / disabled.
  void CheckStatusAndUpdateIcon();

  // The image shown in the tray icon.
  gfx::ImageSkia tray_image_;

  // Bubble view holds additional actions while active.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_TRAY_H_
