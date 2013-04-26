// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_APP_LOGIN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_APP_LOGIN_UI_H_

#include "chrome/browser/chromeos/login/scoped_gaia_auth_extension.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// Login WebUI in interactive identity api sign-in flow for ChromeOS app mode.
// When a kiosk app accesses identity.getAuthToken with {'interactive':true}
// and the app profile is not signed in, this webui will be brought up in
// a modal dialog to authenticate the user. The authentication is carried out
// via the host gaia_auth extension. Upon success, the profile of the webui
// should be populated with proper cookies. Then this UI would fetch the oauth2
// tokens using the cookies and let identity.getAuthToken to continue.
class AppLoginUI : public ui::WebDialogUI {
 public:
  explicit AppLoginUI(content::WebUI* web_ui);
  virtual ~AppLoginUI();

 private:
  ScopedGaiaAuthExtension auth_extension_;

  DISALLOW_COPY_AND_ASSIGN(AppLoginUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_APP_LOGIN_UI_H_
