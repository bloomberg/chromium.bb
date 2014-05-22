// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

class Browser;
class Profile;

// A delegate for the InfoBar shown when the previous session has crashed.
class SessionCrashedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // If |browser| is not incognito, creates a session crashed infobar and
  // delegate and adds the infobar to the InfoBarService for |browser|.
  static void Create(Browser* browser);

 private:
  explicit SessionCrashedInfoBarDelegate(Profile* profile);
  virtual ~SessionCrashedInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  bool accepted_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SessionCrashedInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_INFOBAR_DELEGATE_H_
