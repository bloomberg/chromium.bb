// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_INFOBAR_DELEGATE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class Profile;

// A delegate for the InfoBar shown when the previous session has crashed.
class SessionCrashedInfoBarDelegate : public ConfirmInfoBarDelegate,
                                      public content::NotificationObserver {
 public:
  // If |browser| is not incognito, creates a session crashed infobar delegate
  // and adds it to the InfoBarService for |browser|.
  static void Create(Browser* browser);

 private:
  FRIEND_TEST_ALL_PREFIXES(SessionCrashedInfoBarDelegateUnitTest,
                           DetachingTabWithCrashedInfoBar);
#if defined(UNIT_TEST)
  friend struct base::DefaultDeleter<SessionCrashedInfoBarDelegate>;
#endif

  SessionCrashedInfoBarDelegate(InfoBarService* infobar_service,
                                Profile* profile);
  virtual ~SessionCrashedInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
  bool accepted_;
  bool removed_notification_received_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SessionCrashedInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_INFOBAR_DELEGATE_H_
