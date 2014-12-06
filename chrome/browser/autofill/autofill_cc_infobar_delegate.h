// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_CC_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_CC_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/base/window_open_disposition.h"

class CreditCard;
class PersonalDataManager;
class InfoBarService;

namespace autofill {

// An InfoBar delegate that enables the user to allow or deny storing credit
// card information gathered from a form submission.
class AutofillCCInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an autofill credit card infobar and delegate and adds the infobar
  // to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const base::Closure& save_card_callback);

#if defined(UNIT_TEST)
  static scoped_ptr<ConfirmInfoBarDelegate> Create(
      const base::Closure& save_card_callback) {
    return scoped_ptr<ConfirmInfoBarDelegate>(
        new AutofillCCInfoBarDelegate(save_card_callback));
  }
#endif

 private:
  explicit AutofillCCInfoBarDelegate(const base::Closure& save_card_callback);
  ~AutofillCCInfoBarDelegate() override;

  void LogUserAction(AutofillMetrics::InfoBarMetric user_action);

  // ConfirmInfoBarDelegate:
  void InfoBarDismissed() override;
  int GetIconID() const override;
  Type GetInfoBarType() const override;
  bool ShouldExpireInternal(const NavigationDetails& details) const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  base::string16 GetLinkText() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

  // The callback to save credit card if the user accepts the infobar.
  base::Closure save_card_callback_;

  // Did the user ever explicitly accept or dismiss this infobar?
  bool had_user_interaction_;

  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardInfoBar);

  DISALLOW_COPY_AND_ASSIGN(AutofillCCInfoBarDelegate);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_CC_INFOBAR_DELEGATE_H_
