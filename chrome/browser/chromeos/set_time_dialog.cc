// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/set_time_dialog.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "ui/gfx/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

const int kDefaultWidth = 490;
const int kDefaultHeight = 235;

}  // namespace

// static
void SetTimeDialog::ShowDialog(gfx::NativeWindow owning_window) {
  content::RecordAction(base::UserMetricsAction("Options_SetTimeDialog_Show"));
  chrome::ShowWebDialog(owning_window,
                        ProfileManager::GetActiveUserProfile(),
                        new SetTimeDialog());
}

SetTimeDialog::SetTimeDialog() {
}

SetTimeDialog::~SetTimeDialog() {
}

ui::ModalType SetTimeDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 SetTimeDialog::GetDialogTitle() const {
  return base::string16();
}

GURL SetTimeDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUISetTimeURL);
}

void SetTimeDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void SetTimeDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string SetTimeDialog::GetDialogArgs() const {
  return std::string();
}

void SetTimeDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void SetTimeDialog::OnCloseContents(WebContents* source,
                                    bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool SetTimeDialog::ShouldShowDialogTitle() const {
  return false;
}

bool SetTimeDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

}  // namespace chromeos
