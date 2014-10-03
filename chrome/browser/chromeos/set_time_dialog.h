// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SET_TIME_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_SET_TIME_DIALOG_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/values.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace chromeos {

// Set Time dialog for setting the system time, date and time zone.
class SetTimeDialog : public ui::WebDialogDelegate {
 public:
  SetTimeDialog();
  virtual ~SetTimeDialog();

  static void ShowDialog(gfx::NativeWindow owning_window);

 private:
  // ui::WebDialogDelegate:
  virtual ui::ModalType GetDialogModalType() const override;
  virtual base::string16 GetDialogTitle() const override;
  virtual GURL GetDialogContentURL() const override;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  virtual void GetDialogSize(gfx::Size* size) const override;
  virtual std::string GetDialogArgs() const override;
  virtual void OnDialogClosed(const std::string& json_retval) override;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) override;
  virtual bool ShouldShowDialogTitle() const override;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) override;

  DISALLOW_COPY_AND_ASSIGN(SetTimeDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SET_TIME_DIALOG_H_
