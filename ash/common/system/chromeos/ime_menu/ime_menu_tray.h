// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_MENU_TRAY_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_MENU_TRAY_H_

#include "ash/ash_export.h"
#include "ash/common/system/ime/ime_observer.h"
#include "ash/common/system/tray/ime_info.h"
#include "ash/common/system/tray/tray_background_view.h"
#include "ash/common/system/tray/tray_bubble_wrapper.h"
#include "base/macros.h"
#include "ui/views/bubble/tray_bubble_view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {
class ImeListView;
class StatusAreaWidget;
class WmWindow;

// The tray item for IME menu, which shows the detailed view of a null single
// item.
class ASH_EXPORT ImeMenuTray : public TrayBackgroundView,
                               public IMEObserver,
                               public views::TrayBubbleView::Delegate {
 public:
  explicit ImeMenuTray(WmShelf* wm_shelf);
  ~ImeMenuTray() override;

  // Shows the IME menu bubble and highlights the button.
  void ShowImeMenuBubble();

  // Returns true if the IME menu bubble has been shown.
  bool IsImeMenuBubbleShown();

  // TrayBackgroundView:
  void SetShelfAlignment(ShelfAlignment alignment) override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  bool PerformAction(const ui::Event& event) override;

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_activated) override;

  // views::TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  void OnMouseEnteredView() override;
  void OnMouseExitedView() override;
  base::string16 GetAccessibleNameForBubble() override;
  gfx::Rect GetAnchorRect(views::Widget* anchor_widget,
                          AnchorType anchor_type,
                          AnchorAlignment anchor_alignment) const override;
  void OnBeforeBubbleWidgetInit(
      views::Widget* anchor_widget,
      views::Widget* bubble_widget,
      views::Widget::InitParams* params) const override;
  void HideBubble(const views::TrayBubbleView* bubble_view) override;

 private:
  // To allow the test class to access |label_|.
  friend class ImeMenuTrayTest;

  // Updates the text of the label on the tray.
  void UpdateTrayLabel();

  // Hides the IME menu bubble and lowlights the button.
  void HideImeMenuBubble();

  // Bubble for default and detailed views.
  std::unique_ptr<TrayBubbleWrapper> bubble_;
  ImeListView* ime_list_view_;

  views::Label* label_;
  IMEInfo current_ime_;

  DISALLOW_COPY_AND_ASSIGN(ImeMenuTray);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_MENU_TRAY_H_
