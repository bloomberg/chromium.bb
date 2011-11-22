// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_lock_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/textfield_with_margin.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/browser/chromeos/login/username_view.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/chromeos/views/copy_background.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/native_widget_gtk.h"
#include "views/background.h"
#include "views/border.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/textfield/native_textfield_wrapper.h"
#include "views/controls/textfield/textfield.h"

namespace chromeos {

namespace {

const int kCornerRadius = 5;
const SkColor kPodBackgroundColor = 0xFFF0F0F0;

// A Textfield for password, which also sets focus to itself
// when a mouse is clicked on it. This is necessary in screen locker
// as mouse events are grabbed in the screen locker.
class PasswordField : public TextfieldWithMargin {
 public:
  PasswordField()
      : TextfieldWithMargin(views::Textfield::STYLE_PASSWORD),
        context_menu_disabled_(false) {
    set_text_to_display_when_empty(
        l10n_util::GetStringUTF16(IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT));
  }

  // views::View overrides.
  virtual bool OnMousePressed(const views::MouseEvent& e) {
    RequestFocus();
    return false;
  }

  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE {
    Textfield::ViewHierarchyChanged(is_add, parent, child);
    // Wiat until native widget is created.
    if (!context_menu_disabled_ && native_wrapper_) {
      gfx::NativeView widget = native_wrapper_->GetTestingHandle();
      if (widget) {
        context_menu_disabled_ = true;
        g_signal_connect(widget, "button-press-event",
                         G_CALLBACK(OnButtonPressEventThunk), this);
      }
    }
  }

  CHROMEGTK_CALLBACK_1(PasswordField, gboolean, OnButtonPressEvent,
                       GdkEventButton*);

 private:
  bool context_menu_disabled_;

  DISALLOW_COPY_AND_ASSIGN(PasswordField);
};

gboolean PasswordField::OnButtonPressEvent(GtkWidget* widget,
                                           GdkEventButton* event) {
  // Eat button 2/3 and alt + any button to disable context menu.
  return event->state & GDK_MOD1_MASK ||
      event->button == 2 ||
      event->button == 3;
}

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
                 chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                 content::NotificationService::AllSources());

  user_view_ = new UserView(this,
                            false,  // is_login
                            true);  // need_background
  main_ = new views::View();
  main_->set_background(
      views::Background::CreateSolidBackground(
          kPodBackgroundColor));

  // Password field.
  password_field_ = new PasswordField();

  password_field_->SetController(this);
  password_field_->set_background(new CopyBackground(main_));

  // Setup ThrobberView's host view.
  set_host_view(password_field_);

  // User icon.
  const User& user = screen_locker_->user();
  user_view_->SetImage(user.image(), user.image());

  // User name.
  std::string display_name = user.GetDisplayName();

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = rb.GetFont(ResourceBundle::MediumBoldFont).DeriveFont(
      kSelectedUsernameFontDelta);

  UsernameView* username =
      UsernameView::CreateShapedUsernameView(UTF8ToWide(display_name), false);
  username_ = username;
  username->SetEnabledColor(login::kTextColor);
  username->SetBackgroundColor(main_->background()->get_color());
  username->SetFont(font);

  // Add tooltip if screen name is not unique.
  if (user.NeedsNameTooltip()) {
    string16 tooltip_text = UTF8ToUTF16(user.GetNameTooltip());
    user_view_->SetTooltipText(tooltip_text);
    username->SetTooltipText(tooltip_text);
  }

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

bool ScreenLockView::IsUserSelected() const {
  return true;
}

void ScreenLockView::ContentsChanged(views::Textfield* sender,
                                     const string16& new_contents) {
  if (!new_contents.empty())
    screen_locker_->ClearErrors();
}

bool ScreenLockView::HandleKeyEvent(
    views::Textfield* sender,
    const views::KeyEvent& key_event) {
  screen_locker_->ClearErrors();
  if (key_event.key_code() == ui::VKEY_RETURN) {
    screen_locker_->Authenticate(password_field_->text());
    return true;
  }
  return false;
}

void ScreenLockView::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED || !user_view_)
    return;

  User* user = content::Details<User>(details).ptr();
  if (screen_locker_->user().email() != user->email())
    return;
  user_view_->SetImage(user->image(), user->image());
}

}  // namespace chromeos
