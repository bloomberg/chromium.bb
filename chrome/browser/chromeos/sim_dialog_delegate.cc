// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sim_dialog_delegate.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/url_constants.h"
#include "ui/gfx/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Default width/height of the dialog.
const int kDefaultWidth = 350;
const int kDefaultHeight = 225;

// Width/height for the change PIN dialog mode.
const int kChangePinWidth = 350;
const int kChangePinHeight = 245;

// URL that includes additional mode (other than Unlock flow) that we're using
// dialog for. Possible values:
// change-pin   - use dialog to change PIN, ask for old & new PIN.
// set-lock-on  - enable RequirePin restriction.
// set-lock-off - disable RequirePin restriction.
// In general SIM unlock case sim-unlock URL is loaded w/o parameters.
const char kSimDialogSpecialModeURL[] = "chrome://sim-unlock/?mode=%s";

// Dialog mode constants.
const char kSimDialogChangePinMode[]  = "change-pin";
const char kSimDialogSetLockOnMode[]  = "set-lock-on";
const char kSimDialogSetLockOffMode[] = "set-lock-off";

}  // namespace

namespace chromeos {

// static
void SimDialogDelegate::ShowDialog(gfx::NativeWindow owning_window,
                                   SimDialogMode mode) {
  chrome::ShowWebDialog(owning_window,
                        ProfileManager::GetActiveUserProfile(),
                        new SimDialogDelegate(mode));
}

SimDialogDelegate::SimDialogDelegate(SimDialogMode dialog_mode)
    : dialog_mode_(dialog_mode) {
}

SimDialogDelegate::~SimDialogDelegate() {
}

ui::ModalType SimDialogDelegate::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 SimDialogDelegate::GetDialogTitle() const {
  return base::string16();
}

GURL SimDialogDelegate::GetDialogContentURL() const {
  if (dialog_mode_ == SIM_DIALOG_UNLOCK) {
    std::string url_string(chrome::kChromeUISimUnlockURL);
    return GURL(url_string);
  } else {
    std::string mode_value;
    if (dialog_mode_ == SIM_DIALOG_CHANGE_PIN)
      mode_value = kSimDialogChangePinMode;
    else if (dialog_mode_ == SIM_DIALOG_SET_LOCK_ON)
      mode_value = kSimDialogSetLockOnMode;
    else
      mode_value = kSimDialogSetLockOffMode;
    return GURL(
        base::StringPrintf(kSimDialogSpecialModeURL, mode_value.c_str()));
  }
}

void SimDialogDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void SimDialogDelegate::GetDialogSize(gfx::Size* size) const {
  // TODO(nkostylev): Set custom size based on locale settings.
  if (dialog_mode_ == SIM_DIALOG_CHANGE_PIN)
    size->SetSize(kChangePinWidth , kChangePinHeight);
  else
    size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string SimDialogDelegate::GetDialogArgs() const {
  return "[]";
}

void SimDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void SimDialogDelegate::OnCloseContents(WebContents* source,
                                        bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool SimDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

bool SimDialogDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

}  // namespace chromeos
