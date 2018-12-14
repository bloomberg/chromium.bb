// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_DIALOG_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_DIALOG_CHROMEOS_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

class GURL;

namespace chromeos {

// Extends from |SystemWebDialogDelegate| to create an always-on-top but movable
// dialog. It is intentionally made movable so that users can copy-paste account
// passwords from password managers.
class InlineLoginHandlerDialogChromeOS : public SystemWebDialogDelegate {
 public:
  // Displays the dialog. |email| is an optional parameter that if provided,
  // pre-fills the account email field in the sign-in dialog - useful for
  // account re-authentication.
  static void Show(const std::string& email = std::string());

 protected:
  explicit InlineLoginHandlerDialogChromeOS(const GURL& url);
  ~InlineLoginHandlerDialogChromeOS() override;

  // ui::WebDialogDelegate overrides
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  bool ShouldShowDialogTitle() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InlineLoginHandlerDialogChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_DIALOG_CHROMEOS_H_
