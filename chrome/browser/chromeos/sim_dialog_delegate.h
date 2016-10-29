// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SIM_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_SIM_DIALOG_DELEGATE_H_

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace chromeos {

// SIM unlock dialog displayed in cases when SIM card has to be unlocked.
class SimDialogDelegate : public ui::WebDialogDelegate {
 public:
  // Type of the SIM dialog that is launched.
  typedef enum SimDialogMode {
    SIM_DIALOG_UNLOCK       = 0,  // General unlock flow dialog (PIN/PUK).
    SIM_DIALOG_CHANGE_PIN   = 1,  // Change PIN dialog.
    SIM_DIALOG_SET_LOCK_ON  = 2,  // Enable RequirePin restriction.
    SIM_DIALOG_SET_LOCK_OFF = 3,  // Disable RequirePin restriction.
  } SimDialogMode;

  explicit SimDialogDelegate(SimDialogMode dialog_mode);

  // Shows the SIM unlock dialog box with one of the specified modes. If the
  // |owning_window| is null the dialog is placed in the appropriate modal
  // dialog container on the primary display.
  static void ShowDialog(gfx::NativeWindow owning_window, SimDialogMode mode);

 private:
  ~SimDialogDelegate() override;

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

  SimDialogMode dialog_mode_;

  DISALLOW_COPY_AND_ASSIGN(SimDialogDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SIM_DIALOG_DELEGATE_H_
