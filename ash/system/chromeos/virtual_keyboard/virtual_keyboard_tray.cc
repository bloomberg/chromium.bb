// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/virtual_keyboard/virtual_keyboard_tray.h"

#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {
namespace {

class VirtualKeyboardButton : public views::ImageButton {
 public:
  VirtualKeyboardButton(views::ButtonListener* listener);
  virtual ~VirtualKeyboardButton();

  // Overridden from views::ImageButton:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardButton);
};

VirtualKeyboardButton::VirtualKeyboardButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
}

VirtualKeyboardButton::~VirtualKeyboardButton() {
}

gfx::Size VirtualKeyboardButton::GetPreferredSize() const {
  const int virtual_keyboard_button_height = kShelfSize;
  gfx::Size size = ImageButton::GetPreferredSize();
  int padding = virtual_keyboard_button_height - size.height();
  size.set_height(virtual_keyboard_button_height);
  size.set_width(size.width() + padding);
  return size;
}

}  // namespace

VirtualKeyboardTray::VirtualKeyboardTray(StatusAreaWidget* status_area_widget)
    : TrayBackgroundView(status_area_widget),
      button_(NULL) {
  button_ = new VirtualKeyboardButton(this);
  button_->SetImage(views::CustomButton::STATE_NORMAL,
                    ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                        IDR_AURA_UBER_TRAY_VIRTUAL_KEYBOARD));
  button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                             views::ImageButton::ALIGN_MIDDLE);

  tray_container()->AddChildView(button_);
  SetContentsBackground();
  // The Shell may not exist in some unit tests.
  if (Shell::HasInstance()) {
    Shell::GetInstance()->system_tray_notifier()->
        AddAccessibilityObserver(this);
  }
}

VirtualKeyboardTray::~VirtualKeyboardTray() {
  // The Shell may not exist in some unit tests.
  if (Shell::HasInstance()) {
    Shell::GetInstance()->system_tray_notifier()->
        RemoveAccessibilityObserver(this);
  }
}

void VirtualKeyboardTray::SetShelfAlignment(ShelfAlignment alignment) {
  TrayBackgroundView::SetShelfAlignment(alignment);
  tray_container()->SetBorder(views::Border::NullBorder());
}

base::string16 VirtualKeyboardTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_VIRTUAL_KEYBOARD_TRAY_ACCESSIBLE_NAME);
}

void VirtualKeyboardTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
}

bool VirtualKeyboardTray::ClickedOutsideBubble() {
  return false;
}

bool VirtualKeyboardTray::PerformAction(const ui::Event& event) {
  keyboard::KeyboardController::GetInstance()->ShowKeyboard(true);
  return true;
}

void VirtualKeyboardTray::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK_EQ(button_, sender);
  PerformAction(event);
}

void VirtualKeyboardTray::OnAccessibilityModeChanged(
    AccessibilityNotificationVisibility notify) {
  SetVisible(Shell::GetInstance()->accessibility_delegate()->
      IsVirtualKeyboardEnabled());
}

}  // namespace ash
