// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_cc_infobar_delegate.h"

#include "base/logging.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "grit/components_scaled_resources.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill {

// static
void AutofillCCInfoBarDelegate::Create(
    infobars::InfoBarManager* infobar_manager,
    AutofillClient* autofill_client,
    const base::Closure& save_card_callback) {
  infobar_manager->AddInfoBar(
      infobar_manager->CreateConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new AutofillCCInfoBarDelegate(autofill_client, save_card_callback))));
}

AutofillCCInfoBarDelegate::AutofillCCInfoBarDelegate(
    AutofillClient* autofill_client,
    const base::Closure& save_card_callback)
    : ConfirmInfoBarDelegate(),
      autofill_client_(autofill_client),
      save_card_callback_(save_card_callback),
      had_user_interaction_(false) {
  AutofillMetrics::LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_SHOWN);
}

AutofillCCInfoBarDelegate::~AutofillCCInfoBarDelegate() {
  if (!had_user_interaction_)
    LogUserAction(AutofillMetrics::INFOBAR_IGNORED);
}

void AutofillCCInfoBarDelegate::LogUserAction(
    AutofillMetrics::InfoBarMetric user_action) {
  DCHECK(!had_user_interaction_);

  AutofillMetrics::LogCreditCardInfoBarMetric(user_action);
  had_user_interaction_ = true;
}

infobars::InfoBarDelegate::Type
AutofillCCInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

int AutofillCCInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_AUTOFILL_CC;
}

void AutofillCCInfoBarDelegate::InfoBarDismissed() {
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
}

bool AutofillCCInfoBarDelegate::ShouldExpireInternal(
    const NavigationDetails& details) const {
  // The user has submitted a form, causing the page to navigate elsewhere. We
  // don't want the infobar to be expired at this point, because the user won't
  // get a chance to answer the question.
  return false;
}

base::string16 AutofillCCInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_INFOBAR_TEXT);
}

base::string16 AutofillCCInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_AUTOFILL_CC_INFOBAR_ACCEPT : IDS_AUTOFILL_CC_INFOBAR_DENY);
}

bool AutofillCCInfoBarDelegate::Accept() {
  save_card_callback_.Run();
  save_card_callback_.Reset();
  LogUserAction(AutofillMetrics::INFOBAR_ACCEPTED);
  return true;
}

bool AutofillCCInfoBarDelegate::Cancel() {
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
  return true;
}

base::string16 AutofillCCInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool AutofillCCInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  autofill_client_->LinkClicked(
      GURL(autofill::kHelpURL),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition);

  return false;
}

}  // namespace autofill
