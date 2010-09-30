// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_view.h"

#include "app/l10n_util.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_controller.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "grit/generated_resources.h"
#include "views/focus/focus_manager.h"
#include "views/grid_layout.h"

namespace chromeos {

using login::kBorderSize;
using views::GridLayout;

// NativeButton that will always return focus to password field.
class UserEntryNativeButton : public views::NativeButton {
 public:
  UserEntryNativeButton(UserController* controller,
                        views::ButtonListener* listener,
                        const std::wstring& label)
      : NativeButton(listener, label),
        controller_(controller) {}

  // Overridden from View:
  virtual void DidGainFocus() {
    controller_->FocusPasswordField();
  }

 private:
  UserController* controller_;

  DISALLOW_COPY_AND_ASSIGN(UserEntryNativeButton);
};

// Textfield with custom processing for Tab/Shift+Tab to select entries.
class UserEntryTextfield : public views::Textfield {
 public:
  UserEntryTextfield(UserController* controller,
                     views::Textfield::StyleFlags style)
      : Textfield(style),
        controller_(controller) {}

  // Overridden from View:
  virtual bool OnKeyPressed(const views::KeyEvent& e) {
    if (e.GetKeyCode() == app::VKEY_TAB) {
      int index = controller_->user_index() + (e.IsShiftDown() ? -1 : 1);
      controller_->SelectUser(index, false);
      return true;
    } else {
      return false;
    }
  }

  // Overridden from views::Textfield:
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
    if (e.GetKeyCode() == app::VKEY_TAB)
      return true;
    else
      return views::Textfield::SkipDefaultKeyEventProcessing(e);
  }

 private:
  UserController* controller_;

  DISALLOW_COPY_AND_ASSIGN(UserEntryTextfield);
};


ExistingUserView::ExistingUserView(UserController* uc)
    : password_field_(NULL),
      submit_button_(NULL),
      user_controller_(uc),
      accel_enable_accessibility_(
          WizardAccessibilityHelper::GetAccelerator()) {
  AddAccelerator(accel_login_off_the_record_);
  AddAccelerator(accel_enable_accessibility_);
}

void ExistingUserView::RecreateFields() {
  if (password_field_ == NULL) {
    password_field_ = new UserEntryTextfield(user_controller_,
                                             views::Textfield::STYLE_PASSWORD);
    password_field_->SetFocusable(true);
    password_field_->SetController(user_controller_);
  }
  password_field_->set_text_to_display_when_empty(
      l10n_util::GetStringUTF16(IDS_LOGIN_EMPTY_PASSWORD_TEXT));
  if (submit_button_ && GetFocusManager()) {
    GetFocusManager()->ViewRemoved(this, submit_button_);
  }
  delete submit_button_;
  submit_button_ = new UserEntryNativeButton(
      user_controller_, user_controller_,
      l10n_util::GetString(IDS_LOGIN_BUTTON));
  submit_button_->SetFocusable(false);
  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kBorderSize);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(password_field_);
  layout->AddView(submit_button_);
  Layout();
  SchedulePaint();
}

bool ExistingUserView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (accelerator == accel_login_off_the_record_) {
    user_controller_->OnLoginOffTheRecord();
    return true;
  } else if (accelerator == accel_enable_accessibility_) {
    WizardAccessibilityHelper::GetInstance()->EnableAccessibility(this);
    return true;
  }
  return false;
}

void ExistingUserView::ViewHierarchyChanged(bool is_add,
                                            views::View* parent,
                                            views::View* child) {
  if (is_add && this == child)
    WizardAccessibilityHelper::GetInstance()->MaybeEnableAccessibility(this);
}

void ExistingUserView::FocusPasswordField() {
  if (GetFocusManager()) {
    password_field()->RequestFocus();
  }
}

void ExistingUserView::OnLocaleChanged() {
  RecreateFields();
}

}  // namespace chromeos
