// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/charger_replace/charger_link_dialog.h"

#include "base/json/json_writer.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chromeos/charger_replacement_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

const int kDefaultDialogWidth = 1200;
const int kDefaultDialogHeight = 650;

}  // namespace

ChargerLinkDialog::ChargerLinkDialog(gfx::NativeWindow parent_window,
                                     std::string url)
    : parent_window_(parent_window),
      url_(url) {
}

ChargerLinkDialog::~ChargerLinkDialog() {
}

void ChargerLinkDialog::Show() {
  // We show the dialog for the active user, so that the dialog will get
  // displayed immediately. The parent window is a system modal/lock container
  // and does not belong to any user.
  chrome::ShowWebDialog(parent_window_,
                        ProfileManager::GetActiveUserProfile(),
                        this);
}

ui::ModalType ChargerLinkDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 ChargerLinkDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_DIALOG_TITLE);
}

GURL ChargerLinkDialog::GetDialogContentURL() const {
  return GURL(url_);
}

void ChargerLinkDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void ChargerLinkDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultDialogWidth, kDefaultDialogHeight);
}

std::string ChargerLinkDialog::GetDialogArgs() const {
  return std::string();
}

void ChargerLinkDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void ChargerLinkDialog::OnCloseContents(WebContents* source,
                                               bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool ChargerLinkDialog::ShouldShowDialogTitle() const {
  return true;
}

bool ChargerLinkDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

}  // namespace chromeos
