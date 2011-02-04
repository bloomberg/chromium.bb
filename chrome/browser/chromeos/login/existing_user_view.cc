// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/user_controller.h"
#include "chrome/browser/chromeos/login/textfield_with_margin.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/background.h"
#include "views/focus/focus_manager.h"
#include "views/layout/fill_layout.h"

namespace chromeos {

// Colors for gradient background. These should be consistent with border
// window background so textfield border is not visible to the user.
// The background is needed for password textfield to imitate its borders
// transparency correctly.
const SkColor kBackgroundColorTop = SkColorSetRGB(209, 213, 216);
const SkColor kBackgroundColorBottom = SkColorSetRGB(205, 210, 213);

// Textfield with custom processing for Tab/Shift+Tab to select entries.
class UserEntryTextfield : public TextfieldWithMargin {
 public:
  UserEntryTextfield(UserController* controller,
                     views::Textfield::StyleFlags style)
      : TextfieldWithMargin(style),
        controller_(controller) {}

  // Overridden from views::View:
  virtual bool OnKeyPressed(const views::KeyEvent& e) {
    if (e.GetKeyCode() == ui::VKEY_TAB) {
      controller_->SelectUserRelative(e.IsShiftDown() ? -1 : 1);
      return true;
    } else if (e.GetKeyCode() == ui::VKEY_LEFT) {
      controller_->SelectUserRelative(-1);
      return true;
    } else if (e.GetKeyCode() == ui::VKEY_RIGHT) {
      controller_->SelectUserRelative(1);
      return true;
    } else {
      return false;
    }
  }

  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
    if (e.GetKeyCode() == ui::VKEY_TAB)
      return true;
    else
      return views::Textfield::SkipDefaultKeyEventProcessing(e);
  }

 private:
  UserController* controller_;

  DISALLOW_COPY_AND_ASSIGN(UserEntryTextfield);
};


ExistingUserView::ExistingUserView(UserController* user_controller)
    : user_controller_(user_controller),
      password_field_(NULL),
      accel_login_off_the_record_(
        views::Accelerator(ui::VKEY_B, false, false, true)),
      accel_toggle_accessibility_(
          WizardAccessibilityHelper::GetAccelerator()) {
  AddAccelerator(accel_login_off_the_record_);
  AddAccelerator(accel_toggle_accessibility_);
}

void ExistingUserView::RecreateFields() {
  if (password_field_ == NULL) {
    SetLayoutManager(new views::FillLayout);
    password_field_ = new UserEntryTextfield(user_controller_,
                                             views::Textfield::STYLE_PASSWORD);
    password_field_->set_background(
        views::Background::CreateVerticalGradientBackground(
            kBackgroundColorTop, kBackgroundColorBottom));
    password_field_->SetFocusable(true);
    password_field_->SetController(this);
    AddChildView(password_field_);
  }
  password_field_->set_text_to_display_when_empty(
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT));
  Layout();
  SchedulePaint();
}

bool ExistingUserView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (accelerator == accel_login_off_the_record_) {
    user_controller_->OnLoginAsGuest();
    return true;
  } else if (accelerator == accel_toggle_accessibility_) {
    WizardAccessibilityHelper::GetInstance()->ToggleAccessibility(this);
    return true;
  }
  return false;
}

bool ExistingUserView::HandleKeyEvent(views::Textfield* sender,
                                      const views::KeyEvent& key_event) {
  if (key_event.GetKeyCode() == ui::VKEY_RETURN) {
    if (!password_field_->text().empty())
      user_controller_->OnLogin("", UTF16ToUTF8(password_field_->text()));
  } else {
    user_controller_->ClearErrors();
    return false;
  }
  return true;
}

void ExistingUserView::RequestFocus() {
  password_field_->RequestFocus();
}

void ExistingUserView::ContentsChanged(views::Textfield* sender,
                                       const string16& new_contents) {
  if (!new_contents.empty())
    user_controller_->ClearErrors();
}

void ExistingUserView::EnableInputControls(bool enabled) {
  password_field_->SetEnabled(enabled);
}

void ExistingUserView::ClearAndFocusControls() {
  ClearAndFocusPassword();
}

void ExistingUserView::ClearAndFocusPassword() {
  password_field_->SetText(string16());
  FocusPasswordField();
}

void ExistingUserView::ViewHierarchyChanged(bool is_add,
                                            views::View* parent,
                                            views::View* child) {
  if (is_add && this == child)
    WizardAccessibilityHelper::GetInstance()->MaybeEnableAccessibility(this);
}

void ExistingUserView::FocusPasswordField() {
  password_field_->RequestFocus();
}

gfx::Rect ExistingUserView::GetMainInputScreenBounds() const {
  return password_field_->GetScreenBounds();
}

void ExistingUserView::OnLocaleChanged() {
  RecreateFields();
}

}  // namespace chromeos
