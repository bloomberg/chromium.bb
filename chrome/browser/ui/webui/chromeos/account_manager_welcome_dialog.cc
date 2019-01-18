// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/account_manager_welcome_dialog.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

AccountManagerWelcomeDialog* g_dialog = nullptr;
constexpr int kSigninDialogWidth = 600;
constexpr int kSigninDialogHeight = 500;

}  // namespace

AccountManagerWelcomeDialog::AccountManagerWelcomeDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAccountManagerWelcomeURL),
                              base::string16() /* title */) {}

AccountManagerWelcomeDialog::~AccountManagerWelcomeDialog() {
  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

// static
bool AccountManagerWelcomeDialog::ShowIfRequired() {
  if (g_dialog) {
    // If the dialog is already being displayed, bring it to focus instead of
    // creating another window.
    g_dialog->dialog_window()->Focus();
    return true;
  }

  // Will be deleted by |SystemWebDialogDelegate::OnDialogClosed|.
  g_dialog = new AccountManagerWelcomeDialog();
  g_dialog->ShowSystemDialog();

  // TODO(sinhak): Store the number of times the welcome screen has been shown
  // in Prefs and check against it before showing it again.
  return true;
}

void AccountManagerWelcomeDialog::OnDialogClosed(
    const std::string& json_retval) {
  chrome::SettingsWindowManager::GetInstance()->ShowChromePageForProfile(
      ProfileManager::GetActiveUserProfile(),
      GURL("chrome://settings/accountManager"));

  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

void AccountManagerWelcomeDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kSigninDialogWidth, kSigninDialogHeight);
}

std::string AccountManagerWelcomeDialog::GetDialogArgs() const {
  return std::string();
}

bool AccountManagerWelcomeDialog::ShouldShowDialogTitle() const {
  return false;
}

}  // namespace chromeos
