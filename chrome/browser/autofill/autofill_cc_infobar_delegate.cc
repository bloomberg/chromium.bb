// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"

#include "base/logging.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

AutofillCCInfoBarDelegate::AutofillCCInfoBarDelegate(
    TabContents* tab_contents,
    const CreditCard* credit_card,
    PersonalDataManager* personal_data,
    const AutofillMetrics* metric_logger)
    : ConfirmInfoBarDelegate(tab_contents),
      credit_card_(credit_card),
      personal_data_(personal_data),
      metric_logger_(metric_logger),
      had_user_interaction_(false) {
  metric_logger_->Log(AutofillMetrics::CREDIT_CARD_INFOBAR_SHOWN);
}

AutofillCCInfoBarDelegate::~AutofillCCInfoBarDelegate() {
}

void AutofillCCInfoBarDelegate::LogUserAction(
    AutofillMetrics::CreditCardInfoBarMetric user_action) {
  DCHECK(!had_user_interaction_);

  metric_logger_->Log(user_action);
  had_user_interaction_ = true;
}

bool AutofillCCInfoBarDelegate::ShouldExpire(
    const NavigationController::LoadCommittedDetails& details) const {
  // The user has submitted a form, causing the page to navigate elsewhere. We
  // don't want the infobar to be expired at this point, because the user won't
  // get a chance to answer the question.
  return false;
}

void AutofillCCInfoBarDelegate::InfoBarClosed() {
  if (!had_user_interaction_)
    LogUserAction(AutofillMetrics::CREDIT_CARD_INFOBAR_IGNORED);

  delete this;
}

void AutofillCCInfoBarDelegate::InfoBarDismissed() {
  LogUserAction(AutofillMetrics::CREDIT_CARD_INFOBAR_DENIED);
}

SkBitmap* AutofillCCInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_AUTOFILL);
}

InfoBarDelegate::Type AutofillCCInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 AutofillCCInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_INFOBAR_TEXT);
}

string16 AutofillCCInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_AUTOFILL_CC_INFOBAR_ACCEPT : IDS_AUTOFILL_CC_INFOBAR_DENY);
}

bool AutofillCCInfoBarDelegate::Accept() {
  personal_data_->SaveImportedCreditCard(*credit_card_);
  LogUserAction(AutofillMetrics::CREDIT_CARD_INFOBAR_ACCEPTED);
  return true;
}

bool AutofillCCInfoBarDelegate::Cancel() {
  LogUserAction(AutofillMetrics::CREDIT_CARD_INFOBAR_DENIED);
  return true;
}

string16 AutofillCCInfoBarDelegate::GetLinkText() {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool AutofillCCInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->OpenAutofillHelpTabAndActivate();
  return false;
}
