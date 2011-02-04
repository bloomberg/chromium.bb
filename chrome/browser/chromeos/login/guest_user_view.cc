// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/guest_user_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/user_controller.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

// Button with custom processing for Tab/Shift+Tab to select entries.
class UserEntryButton : public login::WideButton {
 public:
  UserEntryButton(views::ButtonListener* button_listener,
                  UserController* user_controller,
                  const std::wstring& label)
      : WideButton(button_listener, label),
        user_controller_(user_controller) {}

  // Overridden from views::View:
  virtual bool OnKeyPressed(const views::KeyEvent& e) {
    if (e.GetKeyCode() == ui::VKEY_TAB) {
      user_controller_->SelectUserRelative(e.IsShiftDown() ? -1 : 1);
      return true;
    }
    return WideButton::OnKeyPressed(e);
  }

  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
    if (e.GetKeyCode() == ui::VKEY_TAB)
      return true;
    return WideButton::SkipDefaultKeyEventProcessing(e);
  }

 private:
  UserController* user_controller_;

  DISALLOW_COPY_AND_ASSIGN(UserEntryButton);
};


GuestUserView::GuestUserView(UserController* uc)
    : submit_button_(NULL),
      user_controller_(uc),
      accel_toggle_accessibility_(
          WizardAccessibilityHelper::GetAccelerator()),
      accel_login_off_the_record_(
          views::Accelerator(ui::VKEY_B, false, false, true)),
      accel_previous_pod_by_arrow_(
          views::Accelerator(ui::VKEY_LEFT, false, false, false)),
      accel_next_pod_by_arrow_(
          views::Accelerator(ui::VKEY_RIGHT, false, false, false)) {
  AddAccelerator(accel_toggle_accessibility_);
  AddAccelerator(accel_login_off_the_record_);
  AddAccelerator(accel_previous_pod_by_arrow_);
  AddAccelerator(accel_next_pod_by_arrow_);
}

void GuestUserView::RecreateFields() {
  delete submit_button_;
  submit_button_ = new UserEntryButton(
      this,
      user_controller_,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ENTER_GUEST_SESSION_BUTTON)));
  AddChildView(submit_button_);
  Layout();
  SchedulePaint();
}

bool GuestUserView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (accelerator == accel_login_off_the_record_)
    user_controller_->OnLoginAsGuest();
  else if (accelerator == accel_toggle_accessibility_)
    WizardAccessibilityHelper::GetInstance()->ToggleAccessibility(this);
  else if (accelerator == accel_previous_pod_by_arrow_)
    user_controller_->SelectUserRelative(-1);
  else if (accelerator == accel_next_pod_by_arrow_)
    user_controller_->SelectUserRelative(1);
  else
    return false;
  return true;
}

void GuestUserView::RequestFocus() {
  submit_button_->RequestFocus();
}

void GuestUserView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  DCHECK(sender == submit_button_);
  user_controller_->OnLoginAsGuest();
}

void GuestUserView::EnableInputControls(bool enabled) {
  submit_button_->SetEnabled(enabled);
}

void GuestUserView::ViewHierarchyChanged(bool is_add,
                                         views::View* parent,
                                         views::View* child) {
  if (is_add && this == child)
    WizardAccessibilityHelper::GetInstance()->MaybeEnableAccessibility(this);
}

void GuestUserView::OnLocaleChanged() {
  RecreateFields();
}

void GuestUserView::Layout() {
  gfx::Size submit_button_size = submit_button_->GetPreferredSize();
  int submit_button_x = (width() - submit_button_size.width()) / 2;
  int submit_button_y = (height() - submit_button_size.height()) / 2;
  submit_button_->SetBounds(submit_button_x,
                            submit_button_y,
                            submit_button_size.width(),
                            submit_button_size.height());
}

void GuestUserView::ClearAndFocusControls() {
}

void GuestUserView::ClearAndFocusPassword() {
}

gfx::Rect GuestUserView::GetMainInputScreenBounds() const {
  NOTREACHED();
  return gfx::Rect();
}

}  // namespace chromeos
