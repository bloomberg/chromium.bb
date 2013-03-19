// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"

#include "base/logging.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/autofill/browser/credit_card.h"
#include "components/autofill/browser/personal_data_manager.h"
#include "components/autofill/common/autofill_constants.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// static
void AutofillCCInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const AutofillMetrics* metric_logger,
    const base::Closure& save_card_callback) {
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new AutofillCCInfoBarDelegate(
          infobar_service, metric_logger, save_card_callback)));
  metric_logger->LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_SHOWN);
}

// static
scoped_ptr<ConfirmInfoBarDelegate> AutofillCCInfoBarDelegate::CreateForTesting(
    const AutofillMetrics* metric_logger,
    const base::Closure& save_card_callback) {
  metric_logger->LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_SHOWN);
  return scoped_ptr<ConfirmInfoBarDelegate>(
      new AutofillCCInfoBarDelegate(NULL, metric_logger, save_card_callback));
}

AutofillCCInfoBarDelegate::AutofillCCInfoBarDelegate(
    InfoBarService* infobar_service,
    const AutofillMetrics* metric_logger,
    const base::Closure& save_card_callback)
    : ConfirmInfoBarDelegate(infobar_service),
      metric_logger_(metric_logger),
      save_card_callback_(save_card_callback),
      had_user_interaction_(false) {}

AutofillCCInfoBarDelegate::~AutofillCCInfoBarDelegate() {
  if (!had_user_interaction_)
    LogUserAction(AutofillMetrics::INFOBAR_IGNORED);
}

void AutofillCCInfoBarDelegate::LogUserAction(
    AutofillMetrics::InfoBarMetric user_action) {
  DCHECK(!had_user_interaction_);

  metric_logger_->LogCreditCardInfoBarMetric(user_action);
  had_user_interaction_ = true;
}

void AutofillCCInfoBarDelegate::InfoBarDismissed() {
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
}

gfx::Image* AutofillCCInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_AUTOFILL);
}

InfoBarDelegate::Type AutofillCCInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

bool AutofillCCInfoBarDelegate::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  // The user has submitted a form, causing the page to navigate elsewhere. We
  // don't want the infobar to be expired at this point, because the user won't
  // get a chance to answer the question.
  return false;
}

string16 AutofillCCInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_INFOBAR_TEXT);
}

string16 AutofillCCInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
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

string16 AutofillCCInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool AutofillCCInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  owner()->GetWebContents()->GetDelegate()->OpenURLFromTab(
      owner()->GetWebContents(),
      content::OpenURLParams(GURL(components::autofill::kHelpURL),
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             content::PAGE_TRANSITION_LINK,
                             false));
  return false;
}
