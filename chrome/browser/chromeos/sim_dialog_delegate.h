// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SIM_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_SIM_DIALOG_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
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

  // Shows the SIM unlock dialog box with one of the specified modes.
  static void ShowDialog(gfx::NativeWindow owning_window, SimDialogMode mode);

 private:
  virtual ~SimDialogDelegate();

  // Overridden from ui::WebDialogDelegate:
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual base::string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(
      content::WebContents* source, bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;

  SimDialogMode dialog_mode_;

  DISALLOW_COPY_AND_ASSIGN(SimDialogDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SIM_DIALOG_DELEGATE_H_
