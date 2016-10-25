// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_CHOOSE_MOBILE_NETWORK_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_UI_CHOOSE_MOBILE_NETWORK_DIALOG_H_

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace chromeos {

// Dialog for manual selection of cellular network.
class ChooseMobileNetworkDialog : public ui::WebDialogDelegate {
 public:
  // Shows the dialog box. The dialog is created as a child of |parent|. If the
  // |parent| is null the dialog is created in the default modal container.
  static void ShowDialog(gfx::NativeWindow parent);

  // Shows the dialog in an ash window container (which must be a system modal
  // container) on the primary display.
  static void ShowDialogInContainer(int container_id);

 private:
  ChooseMobileNetworkDialog();

  // Overridden from ui::WebDialogDelegate:
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
  bool HandleContextMenu(const content::ContextMenuParams& params) override;

  DISALLOW_COPY_AND_ASSIGN(ChooseMobileNetworkDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_CHOOSE_MOBILE_NETWORK_DIALOG_H_
