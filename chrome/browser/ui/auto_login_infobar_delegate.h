// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTO_LOGIN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTO_LOGIN_INFOBAR_DELEGATE_H_

#include <string>
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "components/auto_login_parser/auto_login_parser.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;
class TokenService;

namespace content {
class NavigationController;
}

// This is the actual infobar displayed to prompt the user to auto-login.
class AutoLoginInfoBarDelegate : public ConfirmInfoBarDelegate,
                                 public content::NotificationObserver {
 public:
  struct Params {
    // Information from a parsed header.
    auto_login_parser::HeaderData header;

    // Username to display in the infobar indicating user to be logged in as.
    // This is initially fetched from sign-in on non-Android platforms. Note
    // that on Android this field is not used.
    std::string username;
  };

  // Creates an autologin infobar delegate and adds it to |infobar_service|.
  static void Create(InfoBarService* infobar_service, const Params& params);

 protected:
  AutoLoginInfoBarDelegate(InfoBarService* owner, const Params& params);
  virtual ~AutoLoginInfoBarDelegate();

 private:
  // Enum values used for UMA histograms.
  enum Actions {
    SHOWN,       // The infobar was shown to the user.
    ACCEPTED,    // The user pressed the accept button.
    REJECTED,    // The user pressed the reject button.
    DISMISSED,   // The user pressed the close button.
    IGNORED,     // The user ignored the infobar.
    LEARN_MORE,  // The user clicked on the learn more link.
    HISTOGRAM_BOUNDING_VALUE
  };

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual AutoLoginInfoBarDelegate* AsAutoLoginInfoBarDelegate() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void RecordHistogramAction(Actions action);

  const Params params_;

  // Whether any UI controls in the infobar were pressed or not.
  bool button_pressed_;

  // For listening to the user signing out.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_AUTO_LOGIN_INFOBAR_DELEGATE_H_
