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
  SimUnlockDialogDelegate();

  // Shows the keyboard overlay dialog box.
  static void ShowDialog(gfx::NativeWindow owning_window);

 private:
  ~SimUnlockDialogDelegate();

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

  DISALLOW_COPY_AND_ASSIGN(SimUnlockDialogDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SIM_UNLOCK_DIALOG_DELEGATE_H_
