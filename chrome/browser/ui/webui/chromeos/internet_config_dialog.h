// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_INTERNET_CONFIG_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_INTERNET_CONFIG_DIALOG_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

class NetworkState;

class InternetConfigDialog : public ui::WebDialogDelegate {
 public:
  // Shows a network configuration dialog for |network_state|.
  static void ShowDialogForNetworkState(
      content::BrowserContext* browser_context,
      int container_id,
      const NetworkState* network_state);
  // Shows a network configuration dialog for a new network of |network_type|.
  static void ShowDialogForNetworkType(content::BrowserContext* browser_context,
                                       int container_id,
                                       const std::string& network_type);

 protected:
  // ui::WebDialogDelegate
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

  InternetConfigDialog(const std::string& network_type,
                       const std::string& network_id);
  ~InternetConfigDialog() override;

 private:
  std::string network_type_;
  std::string network_id_;

  DISALLOW_COPY_AND_ASSIGN(InternetConfigDialog);
};

// A WebUI to host the network configuration UI in a dialog, used in the
// login screen and when a new network is configured from the system tray.
class InternetConfigDialogUI : public ui::WebDialogUI {
 public:
  explicit InternetConfigDialogUI(content::WebUI* web_ui);
  ~InternetConfigDialogUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InternetConfigDialogUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_INTERNET_CONFIG_DIALOG_H_
