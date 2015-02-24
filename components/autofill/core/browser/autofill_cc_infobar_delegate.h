// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CC_INFOBAR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CC_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/base/window_open_disposition.h"

namespace infobars {
class InfoBarManager;
}

namespace autofill {

class AutofillClient;

// An InfoBar delegate that enables the user to allow or deny storing credit
// card information gathered from a form submission.
class AutofillCCInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an autofill credit card infobar and delegate and adds the infobar
  // to |infobar_manager|. The |autofill_client| must outlive the infobar.
  static void Create(infobars::InfoBarManager* infobar_manager,
                     AutofillClient* autofill_client,
                     const base::Closure& save_card_callback);

#if defined(UNIT_TEST)
  static scoped_ptr<ConfirmInfoBarDelegate> Create(
      AutofillClient* autofill_client,
      const base::Closure& save_card_callback) {
    return scoped_ptr<ConfirmInfoBarDelegate>(
        new AutofillCCInfoBarDelegate(autofill_client, save_card_callback));
  }
#endif

 private:
  AutofillCCInfoBarDelegate(AutofillClient* autofill_client,
                            const base::Closure& save_card_callback);
  ~AutofillCCInfoBarDelegate() override;

  void LogUserAction(AutofillMetrics::InfoBarMetric user_action);

  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  int GetIconID() const override;
  void InfoBarDismissed() override;
  bool ShouldExpireInternal(const NavigationDetails& details) const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  base::string16 GetLinkText() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

  // Performs navigation to handle any link click. Guaranteed to outlive us.
  AutofillClient* const autofill_client_;

  // The callback to save credit card if the user accepts the infobar.
  base::Closure save_card_callback_;

  // Did the user ever explicitly accept or dismiss this infobar?
  bool had_user_interaction_;

  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardInfoBar);

  DISALLOW_COPY_AND_ASSIGN(AutofillCCInfoBarDelegate);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CC_INFOBAR_DELEGATE_H_
