// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_INFOBARS_SIMPLE_ALERT_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

class SimpleAlertInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a simple alert infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     int icon_id,  // May be |kNoIconID| if no icon is shown.
                     const base::string16& message,
                     bool auto_expire);

 private:
  SimpleAlertInfoBarDelegate(int icon_id,
                             const base::string16& message,
                             bool auto_expire);
  virtual ~SimpleAlertInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual bool ShouldExpireInternal(
      const NavigationDetails& details) const OVERRIDE;

  const int icon_id_;
  base::string16 message_;
  bool auto_expire_;  // Should it expire automatically on navigation?

  DISALLOW_COPY_AND_ASSIGN(SimpleAlertInfoBarDelegate);
};

#endif  // CHROME_BROWSER_INFOBARS_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
