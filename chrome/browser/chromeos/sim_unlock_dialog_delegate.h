// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SIM_UNLOCK_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_SIM_UNLOCK_DIALOG_DELEGATE_H_

#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

// SIM unlock dialog displayed in cases when SIM card has to be unlocked.
class SimUnlockDialogDelegate : public HtmlDialogUIDelegate {
 public:
  // NOT_CHANGED is used as an indication that SIM RequirePin setting
  // was not requested to be changed. In that case normal SIM unlock flow is
  // executed i.e. entered PIN is used to call EnterPin command.
  // Otherwise, each time user enters PIN, RequirePin command is used and
  // new value of RequirePin setting is passed with the PIN just entered.
  typedef enum SimRequirePin {
    NOT_CHANGED,
    PIN_NOT_REQUIRED,
    PIN_REQUIRED,
  } SimRequirePin;

  SimUnlockDialogDelegate();
  explicit SimUnlockDialogDelegate(SimRequirePin require_pin);

  // Shows the SIM unlock dialog box.
  static void ShowDialog(gfx::NativeWindow owning_window);

  // Shows the SIM unlock dialog box and once user enters PIN,
  // passes that with new setting for RequirePin SIM preference.
  static void ShowDialog(gfx::NativeWindow owning_window, bool require_pin);

 private:
  ~SimUnlockDialogDelegate();

  // Internal implementation that launches SIM unlock dialog.
  static void ShowDialog(gfx::NativeWindow owning_window,
                         SimRequirePin require_pin);

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

  SimRequirePin require_pin_;

  DISALLOW_COPY_AND_ASSIGN(SimUnlockDialogDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SIM_UNLOCK_DIALOG_DELEGATE_H_
