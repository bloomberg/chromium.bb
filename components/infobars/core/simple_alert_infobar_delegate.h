// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
#define COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class InfoBarManager;
}

class SimpleAlertInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a simple alert infobar and delegate and adds the infobar to
  // |infobar_manager|. |icon_id| may be kNoIconID if no icon is shown.
  static void Create(infobars::InfoBarManager* infobar_manager,
                     int icon_id,
                     const base::string16& message,
                     bool auto_expire);

 private:
  SimpleAlertInfoBarDelegate(int icon_id,
                             const base::string16& message,
                             bool auto_expire);
  ~SimpleAlertInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  int GetIconID() const override;
  bool ShouldExpireInternal(const NavigationDetails& details) const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;

  const int icon_id_;
  base::string16 message_;
  bool auto_expire_;  // Should it expire automatically on navigation?

  DISALLOW_COPY_AND_ASSIGN(SimpleAlertInfoBarDelegate);
};

#endif  // COMPONENTS_INFOBARS_CORE_SIMPLE_ALERT_INFOBAR_DELEGATE_H_
