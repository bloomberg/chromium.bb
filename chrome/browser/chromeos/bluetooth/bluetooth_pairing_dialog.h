// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_PAIRING_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_PAIRING_DIALOG_H_

#include "base/macros.h"
#include "base/values.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace device {
class BluetoothDevice;
}

namespace chromeos {

// Bluetooth device pairing dialog shown from ash tray bubble.
class BluetoothPairingDialog : public ui::WebDialogDelegate {
 public:
  explicit BluetoothPairingDialog(const device::BluetoothDevice* device);
  ~BluetoothPairingDialog() override;

  // Shows the dialog in an ash window container (which must be a system modal
  // container) on the primary display. See ash/public/cpp/shell_window_ids.h.
  void ShowInContainer(int container_id);

  content::WebUI* GetWebUIForTest() { return webui_; }

 protected:
  // ui::WebDialogDelegate implementation.
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogShown(content::WebUI* webui,
                     content::RenderViewHost* render_view_host) override;
  // NOTE: This function deletes this object at the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(const content::ContextMenuParams& params) override;

 private:
  base::DictionaryValue device_data_;
  content::WebUI* webui_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothPairingDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_PAIRING_DIALOG_H_
