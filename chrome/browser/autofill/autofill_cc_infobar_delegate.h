// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_CC_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_CC_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "ui/base/window_open_disposition.h"

class CreditCard;
class PersonalDataManager;

namespace content {
struct LoadCommittedDetails;
}

// An InfoBar delegate that enables the user to allow or deny storing credit
// card information gathered from a form submission.
class AutofillCCInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an autofill credit card delegate and adds it to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const CreditCard* credit_card,
                     PersonalDataManager* personal_data,
                     const AutofillMetrics* metric_logger);

#if defined(UNIT_TEST)
  static scoped_ptr<ConfirmInfoBarDelegate> Create(
      const CreditCard* credit_card,
      PersonalDataManager* personal_data,
      const AutofillMetrics* metric_logger) {
    return scoped_ptr<ConfirmInfoBarDelegate>(new AutofillCCInfoBarDelegate(
        NULL, credit_card, personal_data, metric_logger));
  }
#endif

 private:
  AutofillCCInfoBarDelegate(InfoBarService* infobar_service,
                            const CreditCard* credit_card,
                            PersonalDataManager* personal_data,
                            const AutofillMetrics* metric_logger);
  virtual ~AutofillCCInfoBarDelegate();

  void LogUserAction(AutofillMetrics::InfoBarMetric user_action);

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  // The credit card that should be saved if the user accepts the infobar.
  scoped_ptr<const CreditCard> credit_card_;

  // The personal data manager to which the credit card should be saved.
  // Weak reference.
  PersonalDataManager* personal_data_;

  // For logging UMA metrics.
  // Weak reference. Owned by the AutofillManager that initiated this infobar.
  const AutofillMetrics* metric_logger_;

  // Did the user ever explicitly accept or dismiss this infobar?
  bool had_user_interaction_;

  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardInfoBar);

  DISALLOW_COPY_AND_ASSIGN(AutofillCCInfoBarDelegate);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_CC_INFOBAR_DELEGATE_H_
