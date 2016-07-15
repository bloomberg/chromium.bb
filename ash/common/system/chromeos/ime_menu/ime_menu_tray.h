// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_MENU_TRAY_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_MENU_TRAY_H_

#include "ash/ash_export.h"
#include "ash/common/system/ime/ime_observer.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_background_view.h"
#include "base/macros.h"

namespace views {
class Label;
}  // namespace views

namespace ash {
class StatusAreaWidget;
class WmWindow;

// The tray item for IME menu.
class ASH_EXPORT ImeMenuTray : public TrayBackgroundView, public IMEObserver {
 public:
  explicit ImeMenuTray(WmShelf* wm_shelf);
  ~ImeMenuTray() override;

  // TrayBackgroundView:
  void SetShelfAlignment(ShelfAlignment alignment) override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  bool PerformAction(const ui::Event& event) override;

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_activated) override;

 private:
  // To allow the test class to access |label_|.
  friend class ImeMenuTrayTest;

  // Updates the text of the label on the tray.
  void UpdateTrayLabel();

  // Highlights the button and shows the IME menu tray bubble if |is_active| is
  // true; otherwise, lowlights the button and hides the menu.
  void SetTrayActivation(bool is_active);

  views::Label* label_;
  IMEInfo current_ime_;

  DISALLOW_COPY_AND_ASSIGN(ImeMenuTray);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_MENU_TRAY_H_
