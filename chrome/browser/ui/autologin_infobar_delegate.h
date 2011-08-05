// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOLOGIN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOLOGIN_INFOBAR_DELEGATE_H_

#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class PrefService;

namespace net {
class URLRequest;
}

// This class configures an infobar that allows the user to automatically login
// to the currently loaded page with one click.  This is used when the browser
// detects that the user has navigated to a login page and that there are
// stored tokens that would allow a one-click login.
class AutoLoginInfoBarDelegate : public ConfirmInfoBarDelegate,
                                 public NotificationObserver {
 public:
  AutoLoginInfoBarDelegate(TabContentsWrapper* tab_contents_wrapper,
                           const std::string& account,
                           const std::string& args);

  virtual ~AutoLoginInfoBarDelegate();

  // Looks for the X-Auto-Login response header in the request, and if found,
  // displays an infobar in the tab contents identified by the child/route id
  // if possible.
  static void ShowIfAutoLoginRequested(net::URLRequest* request,
                                       int child_id,
                                       int route_id);

 private:
  // ConfirmInfoBarDelegate overrides.
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // NotificationObserver override.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Will display an auto-login infobar in the tab contents that corresponds
  // to the child/route if possible.
  static void ShowInfoBarIfNeeded(const std::string& account,
                                  const std::string& args,
                                  int child_id,
                                  int route_id);

  TabContentsWrapper* tab_contents_wrapper_;
  const std::string account_;
  const std::string args_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_AUTOLOGIN_INFOBAR_DELEGATE_H_
