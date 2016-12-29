// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_MENU_TRAY_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_MENU_TRAY_H_

#include "ash/ash_export.h"
#include "ash/common/system/chromeos/virtual_keyboard/virtual_keyboard_observer.h"
#include "ash/common/system/ime/ime_observer.h"
#include "ash/common/system/tray/ime_info.h"
#include "ash/common/system/tray/tray_background_view.h"
#include "ash/common/system/tray/tray_bubble_wrapper.h"
#include "base/macros.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/views/bubble/tray_bubble_view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {
class ImeListView;

// The tray item for IME menu, which shows the detailed view of a null single
// item.
class ASH_EXPORT ImeMenuTray : public TrayBackgroundView,
                               public IMEObserver,
                               public views::TrayBubbleView::Delegate,
                               public keyboard::KeyboardControllerObserver,
                               public VirtualKeyboardObserver {
 public:
  explicit ImeMenuTray(WmShelf* wm_shelf);
  ~ImeMenuTray() override;

  // Shows the IME menu bubble and highlights the button.
  void ShowImeMenuBubble();

  // Hides the IME menu bubble and lowlights the button.
  void HideImeMenuBubble();

  // Returns true if the IME menu bubble has been shown.
  bool IsImeMenuBubbleShown();

  // Shows the virtual keyboard with the given keyset: emoji, handwriting or
  // voice.
  void ShowKeyboardWithKeyset(const std::string& keyset);

  // Returns true if it should block the auto hide behavior of the shelf.
  bool ShouldBlockShelfAutoHide() const;

  // Returns true if the menu should show emoji, handwriting and voice buttons
  // on the bottom. Otherwise, the menu will show a 'Customize...' bottom row
  // for non-MD UI, and a Settings button in the title row for MD.
  bool ShouldShowEmojiHandwritingVoiceButtons() const;

  // Returns whether the virtual keyboard toggle should be shown in shown in the
  // opt-in IME menu.
  bool ShouldShowKeyboardToggle() const;

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
  void OnBeforeBubbleWidgetInit(
      views::Widget* anchor_widget,
      views::Widget* bubble_widget,
      views::Widget::InitParams* params) const override;
  void HideBubble(const views::TrayBubbleView* bubble_view) override;

  // keyboard::KeyboardControllerObserver:
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;
  void OnKeyboardClosed() override;
  void OnKeyboardHidden() override;

  // VirtualKeyboardObserver:
  void OnKeyboardSuppressionChanged(bool suppressed) override;

 private:
  // To allow the test class to access |label_|.
  friend class ImeMenuTrayTest;

  // Show the IME menu bubble immediately.
  void ShowImeMenuBubbleInternal();

  // Updates the text of the label on the tray.
  void UpdateTrayLabel();

  // Bubble for default and detailed views.
  std::unique_ptr<TrayBubbleWrapper> bubble_;
  ImeListView* ime_list_view_;

  views::Label* label_;
  IMEInfo current_ime_;
  bool show_keyboard_;
  bool force_show_keyboard_;
  bool should_block_shelf_auto_hide_;
  bool keyboard_suppressed_;
  bool show_bubble_after_keyboard_hidden_;

  DISALLOW_COPY_AND_ASSIGN(ImeMenuTray);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_MENU_TRAY_H_
