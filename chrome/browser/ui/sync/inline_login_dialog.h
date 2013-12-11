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

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginDialog);
};

#endif  // CHROME_BROWSER_UI_SYNC_INLINE_LOGIN_DIALOG_H_
