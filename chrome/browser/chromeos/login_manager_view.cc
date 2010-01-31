// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login_manager_view.h"

#include <signal.h>
#include <sys/types.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/keyboard_codes.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/image_background.h"
#include "chrome/browser/chromeos/login_library.h"
#include "chrome/browser/chromeos/network_library.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/label.h"
#include "views/widget/widget.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

using views::Background;
using views::Label;
using views::Textfield;
using views::View;
using views::Widget;

const int kTitleY = 50;
const int kPanelSpacing = 36;
const int kVersionPad = 4;
const int kTextfieldWidth = 286;
const SkColor kVersionColor = 0xFF7691DA;
const SkColor kErrorColor = 0xFF8F384F;
const SkColor kBackground = SK_ColorWHITE;
const char *kDefaultDomain = "@gmail.com";

LoginManagerView::LoginManagerView(int width, int height) {
  dialog_dimensions_.SetSize(width, height);
  Init();
}

LoginManagerView::~LoginManagerView() {
}

void LoginManagerView::Init() {
  username_field_ = new views::Textfield;
  password_field_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);

  os_version_label_ = new views::Label();
  os_version_label_->SetColor(kVersionColor);
  os_version_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  title_label_ = new views::Label();
  title_label_->SetText(l10n_util::GetString(IDS_LOGIN_TITLE));

  error_label_ = new views::Label();
  error_label_->SetColor(kErrorColor);
  error_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  // Creates the main window
  BuildWindow();

  // Controller to handle events from textfields
  username_field_->SetController(this);
  password_field_->SetController(this);
  if (chromeos::LoginLibrary::EnsureLoaded()) {
    loader_.GetVersion(
        &consumer_, NewCallback(this, &LoginManagerView::OnOSVersion));
  }
}

gfx::Size LoginManagerView::GetPreferredSize() {
  return dialog_dimensions_;
}

void LoginManagerView::BuildWindow() {
  View* login_prompt = new View();
  login_prompt->set_background(
      views::Background::CreateSolidBackground(kBackground));
  login_prompt->SetBounds(0,
                          0,
                          dialog_dimensions_.width(),
                          dialog_dimensions_.height());

  int x = (dialog_dimensions_.width() - kTextfieldWidth) / 2;
  int y = kTitleY;
  title_label_->SetBounds(x,
                          y,
                          kTextfieldWidth,
                          title_label_->GetPreferredSize().height());
  y += kPanelSpacing;
  username_field_->SetBounds(x, y, kTextfieldWidth, kPanelSpacing);
  y += 2 * kPanelSpacing;
  password_field_->SetBounds(x, y, kTextfieldWidth, kPanelSpacing);
  y += 2 * kPanelSpacing;
  os_version_label_->SetBounds(
      x,
      y,
      dialog_dimensions_.width() - (x + kVersionPad),
      os_version_label_->GetPreferredSize().height());
  y += kPanelSpacing;
  error_label_->SetBounds(
      x,
      y,
      dialog_dimensions_.width() - (x + kVersionPad),
      error_label_->GetPreferredSize().height());

  login_prompt->AddChildView(title_label_);
  login_prompt->AddChildView(username_field_);
  login_prompt->AddChildView(password_field_);
  login_prompt->AddChildView(os_version_label_);
  login_prompt->AddChildView(error_label_);
  AddChildView(login_prompt);

  if (!chromeos::LoginLibrary::EnsureLoaded()) {
    error_label_->SetText(l10n_util::GetString(IDS_LOGIN_DISABLED_NO_LIBCROS));
    username_field_->SetReadOnly(true);
    password_field_->SetReadOnly(true);
  }

  return;
}

views::View* LoginManagerView::GetContentsView() {
  return this;
}

bool LoginManagerView::Authenticate(const std::string& username,
                                    const std::string& password) {
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

bool LoginManagerView::HandleKeystroke(views::Textfield* s,
    const views::Textfield::Keystroke& keystroke) {
  if (!chromeos::LoginLibrary::EnsureLoaded())
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
    // Disallow 0 size username.
    if (username_field_->text().length() == 0) {
      // Return true so that processing ends
      return true;
    } else {
      std::string username = UTF16ToUTF8(username_field_->text());
      // todo(cmasone) Need to sanitize memory used to store password.
      std::string password = UTF16ToUTF8(password_field_->text());

      if (username.find('@') == std::string::npos) {
        username += kDefaultDomain;
        username_field_->SetText(UTF8ToUTF16(username));
      }

      // Set up credentials to prepare for authentication.
      if (!Authenticate(username, password)) {
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
        return true;
      }
      // TODO(cmasone): something sensible if errors occur.

      SetupSession(username);
      // Return true so that processing ends
      return true;
    }
  }
  // Return false so that processing does not end
  return false;
}

void LoginManagerView::OnOSVersion(
    chromeos::VersionLoader::Handle handle,
    std::string version) {
  os_version_label_->SetText(ASCIIToWide(version));
}
