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
  explicit InlineLoginDialog(Profile* profile);

  // ui::WebDialogDelegate overrides:
  virtual ui::ModalType GetDialogModalType() const override;
  virtual base::string16 GetDialogTitle() const override;
  virtual GURL GetDialogContentURL() const override;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  virtual void GetDialogSize(gfx::Size* size) const override;
  virtual std::string GetDialogArgs() const override;
  virtual void OnDialogClosed(const std::string& json_retval) override;
  virtual void OnCloseContents(
      content::WebContents* source, bool* out_close_dialog) override;
  virtual bool ShouldShowDialogTitle() const override;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) override;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginDialog);
};

#endif  // CHROME_BROWSER_UI_SYNC_INLINE_LOGIN_DIALOG_H_
