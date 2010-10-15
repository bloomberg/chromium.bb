// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_view.h"

#include "app/l10n_util.h"
#include "chrome/browser/chromeos/login/user_controller.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "grit/generated_resources.h"
#include "views/focus/focus_manager.h"
#include "views/grid_layout.h"

namespace chromeos {

using views::GridLayout;

// Textfield with custom processing for Tab/Shift+Tab to select entries.
class UserEntryTextfield : public views::Textfield {
 public:
  UserEntryTextfield(UserController* controller,
                     views::Textfield::StyleFlags style)
      : Textfield(style),
        controller_(controller) {}

  // Overridden from views::View:
  virtual bool OnKeyPressed(const views::KeyEvent& e) {
    if (e.GetKeyCode() == app::VKEY_TAB) {
      int index = controller_->user_index() + (e.IsShiftDown() ? -1 : 1);
      controller_->SelectUser(index);
      return true;
    } else {
      return false;
    }
  }

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
    : accel_login_off_the_record_(
        views::Accelerator(app::VKEY_B, false, false, true)),
      password_field_(NULL),
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
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT));
  GridLayout* layout = new GridLayout(this);
  // TODO(nkostylev): Switch to FillLayout.
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(password_field_);
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
