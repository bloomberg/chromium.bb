// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHOOSE_MOBILE_NETWORK_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_CHOOSE_MOBILE_NETWORK_DIALOG_H_

#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

// Dialog for manual selection of cellular network.
class ChooseMobileNetworkDialog : private HtmlDialogUIDelegate {
 public:
  // Shows the dialog box.
  static void ShowDialog(gfx::NativeWindow owning_window);

 private:
  ChooseMobileNetworkDialog();

  // Overridden from HtmlDialogUI::Delegate:
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(
      TabContents* source, bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChooseMobileNetworkDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHOOSE_MOBILE_NETWORK_DIALOG_H_
