// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_INLINE_LOGIN_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_UI_INLINE_LOGIN_DIALOG_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace content {
class BrowserContext;
}

namespace ui {

class InlineLoginDialog : public WebDialogDelegate {
 public:
  static void Show(content::BrowserContext* context);

 private:
  InlineLoginDialog();
  virtual ~InlineLoginDialog();

  // Overriden from WebDialogDelegate.
  virtual ModalType GetDialogModalType() const OVERRIDE;
  virtual base::string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual bool CanResizeDialog() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginDialog);
};

}  // namespace ui

#endif  // CHROME_BROWSER_CHROMEOS_UI_INLINE_LOGIN_DIALOG_H_
