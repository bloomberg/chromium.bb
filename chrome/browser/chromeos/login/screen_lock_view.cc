// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_lock_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/browser/chromeos/login/username_view.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/chromeos/login/textfield_with_margin.h"
#include "chrome/browser/chromeos/views/copy_background.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/background.h"
#include "views/border.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"

namespace chromeos {

namespace {

const int kCornerRadius = 5;

// A Textfield for password, which also sets focus to itself
// when a mouse is clicked on it. This is necessary in screen locker
// as mouse events are grabbed in the screen locker.
class PasswordField : public TextfieldWithMargin {
 public:
  PasswordField()
      : TextfieldWithMargin(views::Textfield::STYLE_PASSWORD) {
    set_text_to_display_when_empty(
        l10n_util::GetStringUTF16(IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT));
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
      screen_locker_(screen_locker),
      main_(NULL),
      username_(NULL) {
  DCHECK(screen_locker_);
}

ScreenLockView::~ScreenLockView() {
}

gfx::Size ScreenLockView::GetPreferredSize() {
  return main_->GetPreferredSize();
}

void ScreenLockView::Layout() {
  int username_height = login::kSelectedLabelHeight;
  main_->SetBounds(0, 0, width(), height());
  username_->SetBounds(
      kBorderSize,
      login::kUserImageSize - username_height + kBorderSize,
      login::kUserImageSize,
      username_height);
}

void ScreenLockView::Init() {
  registrar_.Add(this,
                 NotificationType::LOGIN_USER_IMAGE_CHANGED,
                 NotificationService::AllSources());

  user_view_ = new UserView(this,
                            false,  // is_login
                            true);  // need_background
  main_ = new views::View();
  // Use rounded rect background.
  views::Painter* painter =
      CreateWizardPainter(&BorderDefinition::kUserBorder);

  main_->set_background(
      views::Background::CreateBackgroundPainter(true, painter));
  main_->set_border(CreateWizardBorder(&BorderDefinition::kUserBorder));

  // Password field.
  password_field_ = new PasswordField();

  password_field_->SetController(this);
  password_field_->set_background(new CopyBackground(main_));

  // Setup ThrobberView's host view.
  set_host_view(password_field_);

  // User icon.
  UserManager::User user = screen_locker_->user();
  user_view_->SetImage(user.image(), user.image());

  // User name.
  std::wstring text = UTF8ToWide(user.GetDisplayName());

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = rb.GetFont(ResourceBundle::MediumBoldFont).DeriveFont(
      kSelectedUsernameFontDelta);

  // Layouts image, textfield and button components.
  GridLayout* layout = new GridLayout(main_);
  main_->SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, kBorderSize);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kBorderSize);

  column_set = layout->AddColumnSet(1);
  column_set->AddPaddingColumn(0, kBorderSize);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kBorderSize);

  layout->AddPaddingRow(0, kBorderSize);
  layout->StartRow(0, 0);
  layout->AddView(user_view_);
  layout->AddPaddingRow(0, kBorderSize);
  layout->StartRow(0, 1);
  layout->AddView(password_field_);
  layout->AddPaddingRow(0, kBorderSize);

  AddChildView(main_);

  UsernameView* username = UsernameView::CreateShapedUsernameView(text, false);
  username_ = username;
  username->SetColor(login::kTextColor);
  username->SetFont(font);
  AddChildView(username);
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
    // TODO(oshima): Re-enabling does not move the focus to the view
    // that had a focus (issue http://crbug.com/43131).
    // Clear focus on the textfield so that re-enabling can set focus
    // back to the text field.
    // FocusManager may be null if the view does not have
    // associated Widget yet.
    if (password_field_->GetFocusManager())
      password_field_->GetFocusManager()->ClearFocus();
  }
  password_field_->SetEnabled(enabled);
}

void ScreenLockView::OnSignout() {
  screen_locker_->Signout();
}

bool ScreenLockView::HandleKeyEvent(
    views::Textfield* sender,
    const views::KeyEvent& key_event) {
  screen_locker_->ClearErrors();
  if (key_event.GetKeyCode() == ui::VKEY_RETURN) {
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
  user_view_->SetImage(user->image(), user->image());
}

void ScreenLockView::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && this == child)
    WizardAccessibilityHelper::GetInstance()->MaybeEnableAccessibility(this);
}

}  // namespace chromeos
