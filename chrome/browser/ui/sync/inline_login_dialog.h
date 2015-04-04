// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_INLINE_LOGIN_DIALOG_H_
#define CHROME_BROWSER_UI_SYNC_INLINE_LOGIN_DIALOG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Profile;

// A dialog to host the inline sign-in WebUI. Currently, it loads
// chrome:://chrome-signin and dismisses itself when the sign-in is finished
// successfully.
class InlineLoginDialog : public ui::WebDialogDelegate {
 public:
  static void Show(Profile* profile);

 private:
  InlineLoginDialog();

  // ui::WebDialogDelegate overrides:
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

  DISALLOW_COPY_AND_ASSIGN(InlineLoginDialog);
};

#endif  // CHROME_BROWSER_UI_SYNC_INLINE_LOGIN_DIALOG_H_
