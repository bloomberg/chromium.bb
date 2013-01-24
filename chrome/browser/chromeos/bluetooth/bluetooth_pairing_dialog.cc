// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_pairing_dialog.h"

#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/url_constants.h"
#include "device/bluetooth/bluetooth_device.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

// Default width/height ratio of screen size.
const int kDefaultWidth = 480;
const int kDefaultHeight = 280;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BluetoothPairingDialog, public:

BluetoothPairingDialog::BluetoothPairingDialog(
    gfx::NativeWindow parent_window,
    const device::BluetoothDevice* device)
    : parent_window_(parent_window) {
  device_data_.SetString("address", device->address());
  device_data_.SetString("name", device->GetName());
  device_data_.SetBoolean("paired", device->IsPaired());
  device_data_.SetBoolean("bonded", device->IsBonded());
  device_data_.SetBoolean("connected", device->IsConnected());
}

BluetoothPairingDialog::~BluetoothPairingDialog() {
}

void BluetoothPairingDialog::Show() {
  chrome::ShowWebDialog(parent_window_,
                        ProfileManager::GetDefaultProfile(),
                        this);
}

///////////////////////////////////////////////////////////////////////////////
// LoginWebDialog, protected:

ui::ModalType BluetoothPairingDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 BluetoothPairingDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_BLUETOOTH_ADD_DEVICE_TITLE);
}

GURL BluetoothPairingDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIBluetoothPairingURL);
}

void BluetoothPairingDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void BluetoothPairingDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string BluetoothPairingDialog::GetDialogArgs() const {
  std::string data;
  base::JSONWriter::Write(&device_data_, &data);
  return data;
}

void BluetoothPairingDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void BluetoothPairingDialog::OnCloseContents(WebContents* source,
                                             bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool BluetoothPairingDialog::ShouldShowDialogTitle() const {
  return true;
}

bool BluetoothPairingDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

}  // namespace chromeos
