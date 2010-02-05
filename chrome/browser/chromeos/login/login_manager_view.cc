// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_manager_view.h"

#include <signal.h>
#include <sys/types.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/keyboard_codes.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/image_background.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/painter.h"
#include "views/widget/widget.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

using views::Background;
using views::Label;
using views::Textfield;
using views::View;
using views::Widget;

static const int kTitleY = 50;
static const int kPanelSpacing = 36;
static const int kVersionPad = 4;
static const int kTextfieldWidth = 286;
static const int kRowPad = 10;
static const int kLabelPad = 2;
static const int kCornerPad = 6;
static const int kCornerRadius = 12;
static const int kShadowOffset = 4;
static const SkColor kErrorColor = 0xFF8F384F;
static const SkColor kBackground = SK_ColorWHITE;
static const SkColor kLabelColor = 0xFF808080;
static const SkColor kVersionColor = 0xFFA0A0A0;
static const char *kDefaultDomain = "@gmail.com";

// Set to true to run on linux and test login.
static const bool kStubOutLogin = false;

// This Painter can be used to draw a background with a shadowed panel with
// rounded corners.
class PanelPainter : public views::Painter {
  static void drawRoundedRect(
      gfx::Canvas* canvas,
      int x, int y,
      int w, int h,
      SkColor color) {
    SkRect rect;
    rect.set(
        SkIntToScalar(x), SkIntToScalar(y),
        SkIntToScalar(x + w), SkIntToScalar(y + h));
    SkPath path;
    path.addRoundRect(
        rect, SkIntToScalar(kCornerRadius), SkIntToScalar(kCornerRadius));
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(color);
    canvas->drawPath(path, paint);
  }

  virtual void Paint(int w, int h, gfx::Canvas* canvas) {
    // Draw shadow, with offset.
    drawRoundedRect(
        canvas,
        kShadowOffset, kShadowOffset,
        w, h,
        SK_ColorBLACK);

    // Draw background, 1 pixel smaller to allow shadow to show.
    drawRoundedRect(
        canvas,
        0, 0,
        w - 1, h - 1,
        kBackground);
  }
};

LoginManagerView::LoginManagerView() {
  Init();
}

void LoginManagerView::Init() {
  // Use rounded rect background.
  set_background(
      views::Background::CreateBackgroundPainter(true, new PanelPainter()));

  // Set up fonts.
  gfx::Font title_font =
      gfx::Font::CreateFont(L"Droid Sans", 10).DeriveFont(0, gfx::Font::BOLD);
  gfx::Font label_font = gfx::Font::CreateFont(L"Droid Sans", 8);
  gfx::Font button_font = label_font;
  gfx::Font field_font = label_font;
  gfx::Font version_font = gfx::Font::CreateFont(L"Droid Sans", 6);

  title_label_ = new views::Label();
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetFont(title_font);
  title_label_->SetText(l10n_util::GetString(IDS_LOGIN_TITLE));
  AddChildView(title_label_);

  username_label_ = new views::Label();
  username_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  username_label_->SetColor(kLabelColor);
  username_label_->SetFont(label_font);
  username_label_->SetText(l10n_util::GetString(IDS_LOGIN_USERNAME));
  AddChildView(username_label_);

  username_field_ = new views::Textfield;
  username_field_->SetFont(field_font);
  AddChildView(username_field_);

  password_label_ = new views::Label();
  password_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  password_label_->SetColor(kLabelColor);
  password_label_->SetFont(label_font);
  password_label_->SetText(l10n_util::GetString(IDS_LOGIN_PASSWORD));
  AddChildView(password_label_);

  password_field_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);
  password_field_->SetFont(field_font);
  AddChildView(password_field_);

  sign_in_button_ = new views::NativeButton(this, std::wstring());
  sign_in_button_->set_font(button_font);
  sign_in_button_->SetLabel(l10n_util::GetString(IDS_LOGIN_BUTTON));
  AddChildView(sign_in_button_);

  os_version_label_ = new views::Label();
  os_version_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  os_version_label_->SetColor(kVersionColor);
  os_version_label_->SetFont(version_font);
  AddChildView(os_version_label_);

  error_label_ = new views::Label();
  error_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  error_label_->SetColor(kErrorColor);
  error_label_->SetFont(label_font);
  AddChildView(error_label_);

  // Restore previously logged in user.
  std::vector<chromeos::UserManager::User> users =
      chromeos::UserManager::Get()->GetUsers();
  if (users.size() > 0) {
    username_field_->SetText(UTF8ToUTF16(users[0].email()));
  }

  // Controller to handle events from textfields
  username_field_->SetController(this);
  password_field_->SetController(this);
  if (chromeos::LoginLibrary::EnsureLoaded()) {
    loader_.GetVersion(
        &consumer_, NewCallback(this, &LoginManagerView::OnOSVersion));
  } else if (!kStubOutLogin) {
    error_label_->SetText(l10n_util::GetString(IDS_LOGIN_DISABLED_NO_LIBCROS));
    username_field_->SetReadOnly(true);
    password_field_->SetReadOnly(true);
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

void LoginManagerView::Layout() {
  // Center the text fields, and align the rest of the views with them.
  int x = (width() - kTextfieldWidth) / 2;
  int y = kTitleY;
  int max_width = width() - (x + kVersionPad);

  y += (setViewBounds(title_label_, x, y, max_width, false) + kRowPad);
  y += (setViewBounds(username_label_, x, y, max_width, false) + kLabelPad);
  y += (setViewBounds(username_field_, x, y, kTextfieldWidth, true) + kRowPad);
  y += (setViewBounds(password_label_, x, y, max_width, false) + kLabelPad);
  y += (setViewBounds(password_field_, x, y, kTextfieldWidth, true) + kRowPad);
  y += (setViewBounds(sign_in_button_, x, y, max_width, false) + kRowPad);
  y += (setViewBounds(error_label_, x, y, max_width, true) + kRowPad);

  setViewBounds(
      os_version_label_,
      kCornerPad,
      height() - (os_version_label_->GetPreferredSize().height() + kCornerPad),
      width() - (2 * kCornerPad),
      true);

  SchedulePaint();
}

gfx::Size LoginManagerView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

views::View* LoginManagerView::GetContentsView() {
  return this;
}

// Sign in button causes a login attempt.
void LoginManagerView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  Login();
}

bool LoginManagerView::Authenticate(const std::string& username,
                                    const std::string& password) {
  if (kStubOutLogin) {
    return true;
  }

  base::ProcessHandle handle;
  std::vector<std::string> argv;
  // TODO(cmasone): we'll want this to be configurable.
  argv.push_back("/opt/google/chrome/session");
  argv.push_back(username);
  argv.push_back(password);

  base::environment_vector no_env;
  base::file_handle_mapping_vector no_files;
  base::LaunchApp(argv, no_env, no_files, false, &handle);
  int child_exit_code;
  return base::WaitForExitCode(handle, &child_exit_code) &&
         child_exit_code == 0;
}

void LoginManagerView::SetupSession(const std::string& username) {
  if (observer_) {
    observer_->OnLogin();
  }
  if (username.find("@google.com") != std::string::npos) {
    // This isn't thread-safe.  However, the login window is specifically
    // supposed to be run in a blocking fashion, before any other threads are
    // created by the initial browser process.
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAutoSSLClientAuth);
  }
  if (chromeos::LoginLibrary::EnsureLoaded())
    chromeos::LoginLibrary::Get()->StartSession(username, "");

}

void LoginManagerView::Login() {
  // Disallow 0 size username.
  if (username_field_->text().empty()) {
    // Return true so that processing ends
    return;
  }
  std::string username = UTF16ToUTF8(username_field_->text());
  // todo(cmasone) Need to sanitize memory used to store password.
  std::string password = UTF16ToUTF8(password_field_->text());

  if (username.find('@') == std::string::npos) {
    username += kDefaultDomain;
    username_field_->SetText(UTF8ToUTF16(username));
  }

  // Set up credentials to prepare for authentication.
  if (Authenticate(username, password)) {
    // TODO(cmasone): something sensible if errors occur.
    SetupSession(username);
    chromeos::UserManager::Get()->UserLoggedIn(username);
  } else {
    chromeos::NetworkLibrary* network = chromeos::NetworkLibrary::Get();
    int errorID;
    // Check networking after trying to login in case user is
    // cached locally or the local admin account.
    if (!network || !network->EnsureLoaded())
      errorID = IDS_LOGIN_ERROR_NO_NETWORK_LIBRARY;
    else if (!network->Connected())
      errorID = IDS_LOGIN_ERROR_NETWORK_NOT_CONNECTED;
    else
      errorID = IDS_LOGIN_ERROR_AUTHENTICATING;
    error_label_->SetText(l10n_util::GetString(errorID));
  }
}

bool LoginManagerView::HandleKeystroke(views::Textfield* s,
    const views::Textfield::Keystroke& keystroke) {
  if (!kStubOutLogin && !chromeos::LoginLibrary::EnsureLoaded())
    return false;

  if (keystroke.GetKeyboardCode() == base::VKEY_TAB) {
    if (username_field_->text().length() != 0) {
      std::string username = UTF16ToUTF8(username_field_->text());

      if (username.find('@') == std::string::npos) {
        username += kDefaultDomain;
        username_field_->SetText(UTF8ToUTF16(username));
      }
      return false;
    }
  } else if (keystroke.GetKeyboardCode() == base::VKEY_RETURN) {
    Login();
    // Return true so that processing ends
    return true;
  }
  // Return false so that processing does not end
  return false;
}

void LoginManagerView::OnOSVersion(
    chromeos::VersionLoader::Handle handle,
    std::string version) {
  os_version_label_->SetText(ASCIIToWide(version));
}
