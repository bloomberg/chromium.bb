// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SIM_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_SIM_DIALOG_DELEGATE_H_

#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

// SIM unlock dialog displayed in cases when SIM card has to be unlocked.
class SimDialogDelegate : public HtmlDialogUIDelegate {
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
  ~SimDialogDelegate();

  // Overridden from HtmlDialogUI::Delegate:
  virtual bool IsDialogModal() const;
  virtual std::wstring GetDialogTitle() const;
  virtual GURL GetDialogContentURL() const;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog);
  virtual bool ShouldShowDialogTitle() const;

  SimDialogMode dialog_mode_;

  DISALLOW_COPY_AND_ASSIGN(SimDialogDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SIM_DIALOG_DELEGATE_H_
