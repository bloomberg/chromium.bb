// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/new_user_view.h"

#include <signal.h>
#include <sys/types.h>

#include <algorithm>
#include <vector>

#include "app/keyboard_codes.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "views/widget/widget_gtk.h"

using views::Label;
using views::Textfield;
using views::View;
using views::WidgetGtk;

namespace {

// NOTE: When adding new controls check RecreateNativeControls()
// that |sign_in_button_| is added with correct index.
const int kSignInButtonFocusOrderIndex = 3;
const int kTextfieldWidth = 286;
const int kRowPad = 7;
const int kColumnPad = 7;
const int kLanguagesMenuWidth = 200;
const int kLanguagesMenuHeight = 30;
const SkColor kErrorColor = 0xFF8F384F;
const char kDefaultDomain[] = "@gmail.com";

// Textfield that adds domain to the entered username if focus is lost and
// username doesn't have full domain.
class UsernameField : public views::Textfield {
 public:
  UsernameField() {}

  // views::Textfield overrides:
  virtual void WillLoseFocus() {
    if (!text().empty()) {
      std::string username = UTF16ToUTF8(text());

      if (username.find('@') == std::string::npos) {
        username += kDefaultDomain;
        SetText(UTF8ToUTF16(username));
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(UsernameField);
};

}  // namespace

namespace chromeos {

NewUserView::NewUserView(Delegate* delegate,
                         bool need_border,
                         bool need_browse_without_signin)
    : username_field_(NULL),
      password_field_(NULL),
      title_label_(NULL),
      sign_in_button_(NULL),
      create_account_link_(NULL),
      browse_without_signin_link_(NULL),
      languages_menubutton_(NULL),
      throbber_(NULL),
      accel_focus_user_(views::Accelerator(app::VKEY_U, false, false, true)),
      accel_focus_pass_(views::Accelerator(app::VKEY_P, false, false, true)),
      delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(focus_grabber_factory_(this)),
      focus_delayed_(false),
      login_in_process_(false),
      need_border_(need_border),
      need_browse_without_signin_(need_browse_without_signin),
      need_create_account_(false) {
}

NewUserView::~NewUserView() {
}

void NewUserView::Init() {
  if (need_border_) {
    // Use rounded rect background.
    set_border(CreateWizardBorder(&BorderDefinition::kScreenBorder));
    views::Painter* painter = CreateWizardPainter(
        &BorderDefinition::kScreenBorder);
    set_background(views::Background::CreateBackgroundPainter(true, painter));
  }

  // Set up fonts.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font = rb.GetFont(ResourceBundle::MediumBoldFont);

  title_label_ = new views::Label();
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetFont(title_font);
  title_label_->SetMultiLine(true);
  AddChildView(title_label_);

  username_field_ = new UsernameField();
  AddChildView(username_field_);

  password_field_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);
  AddChildView(password_field_);

  throbber_ = CreateDefaultSmoothedThrobber();
  AddChildView(throbber_);

  if (need_create_account_) {
    InitLink(&create_account_link_);
  }
  if (need_browse_without_signin_) {
    InitLink(&browse_without_signin_link_);
  }

  language_switch_menu_.InitLanguageMenu();
  languages_menubutton_ = new views::MenuButton(
      NULL, std::wstring(), &language_switch_menu_, true);
  languages_menubutton_->set_menu_marker(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_MENU_DROPARROW_SHARP));
  languages_menubutton_->SetFocusable(true);
  AddChildView(languages_menubutton_);

  AddAccelerator(accel_focus_user_);
  AddAccelerator(accel_focus_pass_);

  UpdateLocalizedStrings();
  RequestFocus();

  // Controller to handle events from textfields
  username_field_->SetController(this);
  password_field_->SetController(this);
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    EnableInputControls(false);
  }
}

bool NewUserView::AcceleratorPressed(const views::Accelerator& accelerator) {
  if (accelerator == accel_focus_user_) {
    username_field_->RequestFocus();
    return true;
  }

  if (accelerator == accel_focus_pass_) {
    password_field_->RequestFocus();
    return true;
  }

  return false;
}

void NewUserView::RecreateNativeControls() {
  // There is no way to get native button preferred size after the button was
  // sized so delete and recreate the button on text update.
  delete sign_in_button_;
  sign_in_button_ = new views::NativeButton(this, std::wstring());
  // Add button after label, user & password fields.
  DCHECK(GetChildViewCount() >= kSignInButtonFocusOrderIndex);
  AddChildView(kSignInButtonFocusOrderIndex, sign_in_button_);
  if (!CrosLibrary::Get()->EnsureLoaded())
    sign_in_button_->SetEnabled(false);
}

void NewUserView::UpdateLocalizedStrings() {
  RecreateNativeControls();

  title_label_->SetText(l10n_util::GetString(IDS_LOGIN_TITLE));
  username_field_->set_text_to_display_when_empty(
      l10n_util::GetStringUTF16(IDS_LOGIN_USERNAME));
  password_field_->set_text_to_display_when_empty(
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD));
  sign_in_button_->SetLabel(l10n_util::GetString(IDS_LOGIN_BUTTON));
  if (need_create_account_) {
    create_account_link_->SetText(
        l10n_util::GetString(IDS_CREATE_ACCOUNT_BUTTON));
  }
  if (need_browse_without_signin_) {
    browse_without_signin_link_->SetText(
        l10n_util::GetString(IDS_BROWSE_WITHOUT_SIGNING_IN_BUTTON));
  }
  delegate_->ClearErrors();
  languages_menubutton_->SetText(language_switch_menu_.GetCurrentLocaleName());
}

void NewUserView::OnLocaleChanged() {
  UpdateLocalizedStrings();
  Layout();
  SchedulePaint();
}

void NewUserView::RequestFocus() {
  MessageLoop::current()->PostTask(FROM_HERE,
      focus_grabber_factory_.NewRunnableMethod(
          &NewUserView::FocusFirstField));
}

void NewUserView::ViewHierarchyChanged(bool is_add,
                                       View *parent,
                                       View *child) {
  if (is_add && child == this) {
    MessageLoop::current()->PostTask(FROM_HERE,
        focus_grabber_factory_.NewRunnableMethod(
            &NewUserView::FocusFirstField));
  }
}

void NewUserView::NativeViewHierarchyChanged(bool attached,
                                             gfx::NativeView native_view,
                                             views::RootView* root_view) {
  if (focus_delayed_ && attached) {
    focus_delayed_ = false;
    MessageLoop::current()->PostTask(FROM_HERE,
        focus_grabber_factory_.NewRunnableMethod(
            &NewUserView::FocusFirstField));
  }
}

void NewUserView::FocusFirstField() {
  if (GetFocusManager()) {
    if (username_field_->text().empty())
      username_field_->RequestFocus();
    else
      password_field_->RequestFocus();
  } else {
    // We are invisible - delay until it is no longer the case.
    focus_delayed_ = true;
  }
}

// Sets the bounds of the view, using x and y as the origin.
// The width is determined by the min of width and the preferred size
// of the view, unless force_width is true in which case it is always used.
// The height is gotten from the preferred size and returned.
static int setViewBounds(
    views::View* view, int x, int y, int width, bool force_width) {
  gfx::Size pref_size = view->GetPreferredSize();
  if (!force_width) {
    if (pref_size.width() < width) {
      width = pref_size.width();
    }
  }
  int height = pref_size.height();
  view->SetBounds(x, y, width, height);
  return height;
}

void NewUserView::Layout() {
  gfx::Insets insets = GetInsets();

  // Place language selection in top right corner.
  int x = std::max(0,
      this->width() - insets.right() - kLanguagesMenuWidth - kColumnPad);
  int y = insets.top() + kRowPad;
  int width = std::min(this->width() - insets.width() - 2 * kColumnPad,
                       kLanguagesMenuWidth);
  int height = kLanguagesMenuHeight;
  languages_menubutton_->SetBounds(x, y, width, height);

  width = std::min(this->width() - insets.width() - 2 * kColumnPad,
                   kTextfieldWidth);
  x = (this->width() - width) / 2;
  int max_width = this->width() - x - insets.right();
  title_label_->SizeToFit(max_width);

  int create_account_link_height = need_create_account_ ?
      create_account_link_->GetPreferredSize().height() : 0;
  int browse_without_signin_link_height = need_browse_without_signin_ ?
      browse_without_signin_link_->GetPreferredSize().height() : 0;

  height = title_label_->GetPreferredSize().height() +
           username_field_->GetPreferredSize().height() +
           password_field_->GetPreferredSize().height() +
           sign_in_button_->GetPreferredSize().height() +
           create_account_link_height +
           browse_without_signin_link_height +
           4 * kRowPad;
  y = (this->height() - height) / 2;

  y += (setViewBounds(title_label_, x, y, max_width, false) + kRowPad);
  y += (setViewBounds(username_field_, x, y, width, true) + kRowPad);
  y += (setViewBounds(password_field_, x, y, width, true) + kRowPad);
  int throbber_y = y;
  y += (setViewBounds(sign_in_button_, x, y, width, false) + kRowPad);
  setViewBounds(throbber_,
                x + width - throbber_->GetPreferredSize().width(),
                throbber_y + (sign_in_button_->GetPreferredSize().height() -
                              throbber_->GetPreferredSize().height()) / 2,
                width,
                false);
  if (need_create_account_) {
    y += setViewBounds(create_account_link_, x, y, max_width, false);
  }

  if (need_browse_without_signin_) {
    y += setViewBounds(browse_without_signin_link_, x, y, max_width, false);
  }
  SchedulePaint();
}

gfx::Size NewUserView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

void NewUserView::SetUsername(const std::string& username) {
  username_field_->SetText(UTF8ToUTF16(username));
}

void NewUserView::SetPassword(const std::string& password) {
  password_field_->SetText(UTF8ToUTF16(password));
}

void NewUserView::Login() {
  if (login_in_process_ || username_field_->text().empty())
    return;

  throbber_->Start();
  login_in_process_ = true;
  EnableInputControls(false);
  std::string username = UTF16ToUTF8(username_field_->text());
  // todo(cmasone) Need to sanitize memory used to store password.
  std::string password = UTF16ToUTF8(password_field_->text());

  if (username.find('@') == std::string::npos) {
    username += kDefaultDomain;
    username_field_->SetText(UTF8ToUTF16(username));
  }

  delegate_->OnLogin(username, password);
}

// Sign in button causes a login attempt.
void NewUserView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  DCHECK(sender == sign_in_button_);
  Login();
}

void NewUserView::LinkActivated(views::Link* source, int event_flags) {
  if (source == create_account_link_) {
    delegate_->OnCreateAccount();
  } else if (source == browse_without_signin_link_) {
    delegate_->OnLoginOffTheRecord();
  }
}

void NewUserView::ClearAndEnablePassword() {
  login_in_process_ = false;
  EnableInputControls(true);
  SetPassword(std::string());
  password_field_->RequestFocus();
  throbber_->Stop();
}

void NewUserView::ClearAndEnableFields() {
  login_in_process_ = false;
  EnableInputControls(true);
  SetUsername(std::string());
  SetPassword(std::string());
  username_field_->RequestFocus();
  throbber_->Stop();
}

gfx::Rect NewUserView::GetPasswordBounds() const {
  return password_field_->GetScreenBounds();
}

gfx::Rect NewUserView::GetUsernameBounds() const {
  return username_field_->GetScreenBounds();
}

void NewUserView::StopThrobber() {
  throbber_->Stop();
}

bool NewUserView::HandleKeystroke(views::Textfield* s,
    const views::Textfield::Keystroke& keystroke) {
  if (!CrosLibrary::Get()->EnsureLoaded() || login_in_process_)
    return false;

  if (keystroke.GetKeyboardCode() == app::VKEY_RETURN) {
    Login();
    // Return true so that processing ends
    return true;
  } else if (keystroke.GetKeyboardCode() == app::VKEY_LEFT) {
    if (s == username_field_ &&
        username_field_->text().empty() &&
        password_field_->text().empty()) {
      delegate_->NavigateAway();
      return true;
    }
  }
  delegate_->ClearErrors();
  // Return false so that processing does not end
  return false;
}

void NewUserView::EnableInputControls(bool enabled) {
  languages_menubutton_->SetEnabled(enabled);
  username_field_->SetEnabled(enabled);
  password_field_->SetEnabled(enabled);
  sign_in_button_->SetEnabled(enabled);
  if (need_create_account_) {
    create_account_link_->SetEnabled(enabled);
  }
  if (need_browse_without_signin_) {
    browse_without_signin_link_->SetEnabled(enabled);
  }
}

void NewUserView::InitLink(views::Link** link) {
  *link = new views::Link(std::wstring());
  (*link)->SetController(this);
  AddChildView(*link);
}

}  // namespace chromeos
