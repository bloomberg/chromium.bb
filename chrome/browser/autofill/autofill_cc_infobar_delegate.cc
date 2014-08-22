// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"

#include "base/logging.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// static
void AutofillCCInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const AutofillMetrics* metric_logger,
    const base::Closure& save_card_callback) {
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new AutofillCCInfoBarDelegate(
          metric_logger, save_card_callback))));
}

AutofillCCInfoBarDelegate::AutofillCCInfoBarDelegate(
    const AutofillMetrics* metric_logger,
    const base::Closure& save_card_callback)
    : ConfirmInfoBarDelegate(),
      metric_logger_(metric_logger),
      save_card_callback_(save_card_callback),
      had_user_interaction_(false) {
  metric_logger->LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_SHOWN);
}

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

int AutofillCCInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_AUTOFILL_CC;
}

infobars::InfoBarDelegate::Type AutofillCCInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
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
  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
          GURL(autofill::kHelpURL), content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          content::PAGE_TRANSITION_LINK, false));
  return false;
}

}  // namespace autofill
