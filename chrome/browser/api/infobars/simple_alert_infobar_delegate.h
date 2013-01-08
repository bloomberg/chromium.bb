// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_API_INFOBARS_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_API_INFOBARS_SIMPLE_ALERT_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"

class SimpleAlertInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a simple alert delegate and adds it to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     gfx::Image* icon,  // May be NULL.
                     const string16& message,
                     bool auto_expire);

 private:
  SimpleAlertInfoBarDelegate(InfoBarService* infobar_service,
                             gfx::Image* icon,
                             const string16& message,
                             bool auto_expire);
  virtual ~SimpleAlertInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;

  gfx::Image* icon_;
  string16 message_;
  bool auto_expire_;  // Should it expire automatically on navigation?

  DISALLOW_COPY_AND_ASSIGN(SimpleAlertInfoBarDelegate);
};

#endif  // CHROME_BROWSER_API_INFOBARS_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
