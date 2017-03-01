// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_pairing_dialog.h"

#include <string>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/json/json_writer.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "device/bluetooth/bluetooth_device.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"

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
    const device::BluetoothDevice* device)
    : webui_(nullptr) {
  device_data_.SetString("address", device->GetAddress());
  device_data_.SetString("name", device->GetNameForDisplay());
  device_data_.SetBoolean("paired", device->IsPaired());
  device_data_.SetBoolean("connected", device->IsConnected());
}

BluetoothPairingDialog::~BluetoothPairingDialog() {
}

void BluetoothPairingDialog::ShowInContainer(int container_id) {
  // Dialog must be in a modal window container.
  DCHECK(container_id == ash::kShellWindowId_SystemModalContainer ||
         container_id == ash::kShellWindowId_LockSystemModalContainer);

  // This block of code is almost the same as chrome::ShowWebDialogInContainer()
  // except we're creating a frameless window.
  views::Widget* widget = new views::Widget;  // Owned by native widget
  views::WebDialogView* view =
      new views::WebDialogView(ProfileManager::GetActiveUserProfile(), this,
                               new ChromeWebContentsHandler);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = view;
  if (ash_util::IsRunningInMash()) {
    using ui::mojom::WindowManager;
    params.mus_properties[WindowManager::kContainerId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(container_id);
  } else {
    params.parent = ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                             container_id);
  }
  widget->Init(params);

  // Observer is needed for ChromeVox extension to send messages between content
  // and background scripts.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      view->web_contents());

  widget->Show();
}

///////////////////////////////////////////////////////////////////////////////
// LoginWebDialog, protected:

ui::ModalType BluetoothPairingDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 BluetoothPairingDialog::GetDialogTitle() const {
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
  base::JSONWriter::Write(device_data_, &data);
  return data;
}

void BluetoothPairingDialog::OnDialogShown(
    content::WebUI* webui,
    content::RenderViewHost* render_view_host) {
  webui_ = webui;
}

void BluetoothPairingDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void BluetoothPairingDialog::OnCloseContents(WebContents* source,
                                             bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool BluetoothPairingDialog::ShouldShowDialogTitle() const {
  return false;
}

bool BluetoothPairingDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

}  // namespace chromeos
