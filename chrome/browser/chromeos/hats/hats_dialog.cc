// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_dialog.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

// Default width/height ratio of screen size.
const int kDefaultWidth = 400;
const int kDefaultHeight = 420;

}  // namespace

// HatsDialog, public:

HatsDialog::HatsDialog() {}

HatsDialog::~HatsDialog() {}

void HatsDialog::Show() {
  chrome::ShowWebDialog(nullptr, ProfileManager::GetActiveUserProfile(), this);
}

ui::ModalType HatsDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 HatsDialog::GetDialogTitle() const {
  return base::string16();
}

GURL HatsDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIHatsURL);
}

void HatsDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {}

void HatsDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

bool HatsDialog::CanResizeDialog() const {
  return false;
}

std::string HatsDialog::GetDialogArgs() const {
  return std::string();
}

void HatsDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void HatsDialog::OnCloseContents(WebContents* source, bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool HatsDialog::ShouldShowDialogTitle() const {
  return false;
}

bool HatsDialog::HandleContextMenu(const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

}  // namespace chromeos
