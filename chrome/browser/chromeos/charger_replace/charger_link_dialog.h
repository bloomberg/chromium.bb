// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHARGER_REPLACE_CHARGER_LINK_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_CHARGER_REPLACE_CHARGER_LINK_DIALOG_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace chromeos {

// Dialog for showing the web page for the links on charger replacement dialog.
class ChargerLinkDialog : public ui::WebDialogDelegate {
 public:
  ChargerLinkDialog(gfx::NativeWindow parent_window,
                    std::string url);
  virtual ~ChargerLinkDialog();

  void Show();

 private:
  // ui::WebDialogDelegate implementation.
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual base::string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  // NOTE: This function deletes this object at the end.
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(
      content::WebContents* source, bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;

  gfx::NativeWindow parent_window_;
  std::string url_;

  DISALLOW_COPY_AND_ASSIGN(ChargerLinkDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHARGER_REPLACE_CHARGER_LINK_DIALOG_H_
