// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_lock_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/border.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"

namespace chromeos {

namespace {

// A Textfield for password, which also sets focus to itself
// when a mouse is clicked on it. This is necessary in screen locker
// as mouse events are grabbed in the screen locker.
class PasswordField : public views::Textfield {
 public:
  PasswordField()
      : views::Textfield(views::Textfield::STYLE_PASSWORD) {
    set_text_to_display_when_empty(
        l10n_util::GetStringUTF16(IDS_LOGIN_EMPTY_PASSWORD_TEXT));
  }

  // views::View overrides.
  virtual bool OnMousePressed(const views::MouseEvent& e) {
    RequestFocus();
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordField);
};

}  // namespace

using views::GridLayout;
using login::kBorderSize;

ScreenLockView::ScreenLockView(ScreenLocker* screen_locker)
    : user_view_(NULL),
      password_field_(NULL),
      unlock_button_(NULL),
      screen_locker_(screen_locker) {
  DCHECK(screen_locker_);
}

void ScreenLockView::Init() {
  registrar_.Add(this,
                 NotificationType::LOGIN_USER_IMAGE_CHANGED,
                 NotificationService::AllSources());

  user_view_ = new UserView(this, false);
  views::View* main = new views::View();
  main->set_background(
      views::Background::CreateSolidBackground(login::kBackgroundColor));

  // Password field.
  password_field_ = new PasswordField();
  password_field_->SetController(this);

  // Unlock button.
  unlock_button_ = new views::TextButton(
      this, l10n_util::GetString(IDS_UNLOCK_BUTTON));
  unlock_button_->set_tag(login::UNLOCK);

  // User icon.
  UserManager::User user = screen_locker_->user();
  user_view_->SetImage(user.image());

  // User name.
  std::wstring text = UTF8ToWide(user.GetDisplayName());
  views::Label* label = new views::Label(text);
  label->SetColor(login::kTextColor);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font =
      rb.GetFont(ResourceBundle::LargeFont).DeriveFont(0, gfx::Font::BOLD);
  label->SetFont(font);

  // Layouts image, textfield and button components.
  GridLayout* layout = new GridLayout(main);
  main->SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, kBorderSize);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kBorderSize);

  column_set = layout->AddColumnSet(1);
  column_set->AddPaddingColumn(0, 5);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 5);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 5);

  layout->AddPaddingRow(0, kBorderSize);
  layout->StartRow(0, 0);
  layout->AddView(user_view_);
  layout->AddPaddingRow(0, kBorderSize);
  layout->StartRow(0, 1);
  layout->AddView(password_field_);
  layout->AddView(unlock_button_);
  layout->AddPaddingRow(0, 5);

  unlock_button_->SetFocusable(true);

  // Layouts the main view and the account label.
  layout = new GridLayout(this);
  SetLayoutManager(layout);
  column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(1);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(main);
  layout->StartRow(0, 1);
  layout->AddView(label);
}

void ScreenLockView::ClearAndSetFocusToPassword() {
  password_field_->RequestFocus();
  password_field_->SetText(string16());
}

void ScreenLockView::SetSignoutEnabled(bool enabled) {
  user_view_->SetSignoutEnabled(enabled);
}

gfx::Rect ScreenLockView::GetPasswordBoundsRelativeTo(const views::View* view) {
  gfx::Point p;
  views::View::ConvertPointToView(password_field_, view, &p);
  return gfx::Rect(p, size());
}

void ScreenLockView::SetEnabled(bool enabled) {
  views::View::SetEnabled(enabled);

  if (!enabled) {
    user_view_->StartThrobber();
    // TODO(oshima): Re-enabling does not move the focus to the view
    // that had a focus (issue http://crbug.com/43131).
    // Move the focus to other field as a workaround.
    unlock_button_->RequestFocus();
  } else {
    user_view_->StopThrobber();
  }
  unlock_button_->SetEnabled(enabled);
  password_field_->SetEnabled(enabled);
}

void ScreenLockView::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  if (sender->tag() == login::UNLOCK)
    screen_locker_->Authenticate(password_field_->text());
  else
    NOTREACHED();
}

void ScreenLockView::OnSignout() {
  screen_locker_->Signout();
}

bool ScreenLockView::HandleKeystroke(
    views::Textfield* sender,
    const views::Textfield::Keystroke& keystroke) {
  screen_locker_->ClearErrors();
  if (keystroke.GetKeyboardCode() == base::VKEY_RETURN) {
    screen_locker_->Authenticate(password_field_->text());
    return true;
  }
  return false;
}

void ScreenLockView::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type != NotificationType::LOGIN_USER_IMAGE_CHANGED || !user_view_)
    return;

  UserManager::User* user = Details<UserManager::User>(details).ptr();
  if (screen_locker_->user().email() != user->email())
    return;

  user_view_->SetImage(user->image());
}

}  // namespace chromeos
