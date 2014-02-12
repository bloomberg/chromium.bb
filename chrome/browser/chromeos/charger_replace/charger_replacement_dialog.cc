// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/charger_replace/charger_replacement_dialog.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chromeos/charger_replacement_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

const int kDefaultDialogWidth = 500;
const int kDefaultDialogHeight = 590;
const int kMinDialogWidth = 100;
const int kMinDialogHeight = 100;

const char kNewChargerOrdered[] = "1";
const char kNewChargerNotOrdered[] = "0";

}  // namespace

// static member variable.
bool ChargerReplacementDialog::is_window_visible_ = false;
gfx::NativeWindow ChargerReplacementDialog::current_window_ = NULL;

ChargerReplacementDialog::ChargerReplacementDialog(
    gfx::NativeWindow parent_window)
    : parent_window_(parent_window),
      can_close_(false),
      charger_replacement_handler_(new ChargerReplacementHandler(this)) {
}

ChargerReplacementDialog::~ChargerReplacementDialog() {
  is_window_visible_ = false;
  current_window_ = NULL;
}

// static
bool ChargerReplacementDialog::ShouldShowDialog() {
  if (is_window_visible_)
    return false;

  ChargerReplacementHandler::SpringChargerStatus charger_status =
      ChargerReplacementHandler::GetChargerStatusPref();

  return (charger_status == ChargerReplacementHandler::CHARGER_UNKNOWN ||
      charger_status ==
          ChargerReplacementHandler::CONFIRM_NEW_CHARGER_ORDERED_ONLINE ||
      charger_status ==
          ChargerReplacementHandler::CONFIRM_ORDER_NEW_CHARGER_BY_PHONE);
}

// static
bool ChargerReplacementDialog::IsDialogVisible() {
  return is_window_visible_;
}

void ChargerReplacementDialog::SetFocusOnChargerDialogIfVisible() {
  if (is_window_visible_ && current_window_)
    current_window_->Focus();
}

void ChargerReplacementDialog::Show() {
  content::RecordAction(
        base::UserMetricsAction("ShowChargerReplacementDialog"));

  is_window_visible_ = true;
  // We show the dialog for the active user, so that the dialog will get
  // displayed immediately. The parent window is a system modal/lock container
  // and does not belong to any user.
  current_window_ = chrome::ShowWebDialog(
      parent_window_,
      ProfileManager::GetActiveUserProfile(),
      this);
  charger_replacement_handler_->set_charger_window(current_window_);
}

ui::ModalType ChargerReplacementDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 ChargerReplacementDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_DIALOG_TITLE);
}

GURL ChargerReplacementDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIChargerReplacementURL);
}

void ChargerReplacementDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(charger_replacement_handler_);
}

void ChargerReplacementDialog::GetMinimumDialogSize(gfx::Size* size) const {
  size->SetSize(kMinDialogWidth, kMinDialogHeight);
}

void ChargerReplacementDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultDialogWidth, kDefaultDialogHeight);
}

std::string ChargerReplacementDialog::GetDialogArgs() const {
  ChargerReplacementHandler::SpringChargerStatus charger_status =
      ChargerReplacementHandler::GetChargerStatusPref();
  if (charger_status ==
          ChargerReplacementHandler::CONFIRM_NEW_CHARGER_ORDERED_ONLINE ||
      charger_status ==
          ChargerReplacementHandler::CONFIRM_ORDER_NEW_CHARGER_BY_PHONE) {
    return std::string(kNewChargerOrdered);
  } else {
    return std::string(kNewChargerNotOrdered);
  }
}

bool ChargerReplacementDialog::CanCloseDialog() const {
  return can_close_;
}

void ChargerReplacementDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void ChargerReplacementDialog::OnCloseContents(WebContents* source,
                                               bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool ChargerReplacementDialog::ShouldShowDialogTitle() const {
  return true;
}

bool ChargerReplacementDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

}  // namespace chromeos
