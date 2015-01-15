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
  ~InlineLoginDialog() override;

  // Overriden from WebDialogDelegate.
  ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  bool CanResizeDialog() const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginDialog);
};

}  // namespace ui

#endif  // CHROME_BROWSER_CHROMEOS_UI_INLINE_LOGIN_DIALOG_H_
