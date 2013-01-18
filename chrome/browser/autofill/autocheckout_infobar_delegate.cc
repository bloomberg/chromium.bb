// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autocheckout_infobar_delegate.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/autofill/autocheckout_manager.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// static
void AutocheckoutInfoBarDelegate::Create(
    const AutofillMetrics& metric_logger,
    const GURL& source_url,
    const content::SSLStatus& ssl_status,
    AutocheckoutManager* autocheckout_manager,
    InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new AutocheckoutInfoBarDelegate(metric_logger, source_url, ssl_status,
          autocheckout_manager, infobar_service)));
}

AutocheckoutInfoBarDelegate::AutocheckoutInfoBarDelegate(
    const AutofillMetrics& metric_logger,
    const GURL& source_url,
    const content::SSLStatus& ssl_status,
    AutocheckoutManager* autocheckout_manager,
    InfoBarService* infobar_service)
    : ConfirmInfoBarDelegate(infobar_service),
      metric_logger_(metric_logger),
      autocheckout_manager_(autocheckout_manager),
      source_url_(source_url),
      ssl_status_(ssl_status),
      had_user_interaction_(false) {
  metric_logger_.LogAutocheckoutInfoBarMetric(AutofillMetrics::INFOBAR_SHOWN);
}

AutocheckoutInfoBarDelegate::~AutocheckoutInfoBarDelegate() {
  if (!had_user_interaction_)
    LogUserAction(AutofillMetrics::INFOBAR_IGNORED);
}

void AutocheckoutInfoBarDelegate::LogUserAction(
    AutofillMetrics::InfoBarMetric user_action) {
  DCHECK(!had_user_interaction_);
  metric_logger_.LogAutocheckoutInfoBarMetric(user_action);
  had_user_interaction_ = true;
}

void AutocheckoutInfoBarDelegate::InfoBarDismissed() {
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
}

gfx::Image* AutocheckoutInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_AUTOFILL);
}

InfoBarDelegate::Type AutocheckoutInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

bool AutocheckoutInfoBarDelegate::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  // The user has submitted a form, causing the page to navigate elsewhere. We
  // want the infobar to be expired at this point, because the user has
  // potentially started the checkout flow manually.
  return true;
}


string16 AutocheckoutInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_FLOW_INFOBAR_TEXT);
}

string16 AutocheckoutInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_AUTOFILL_FLOW_INFOBAR_ACCEPT : IDS_AUTOFILL_FLOW_INFOBAR_DENY);
}

bool AutocheckoutInfoBarDelegate::Accept() {
  LogUserAction(AutofillMetrics::INFOBAR_ACCEPTED);
  autocheckout_manager_->ShowAutocheckoutDialog(source_url_, ssl_status_);
  return true;
}

bool AutocheckoutInfoBarDelegate::Cancel() {
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
  return true;
}

string16 AutocheckoutInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool AutocheckoutInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  // TODO(ramankk): Fix the help URL when we have one.
  owner()->GetWebContents()->GetDelegate()->OpenURLFromTab(
      owner()->GetWebContents(),
      content::OpenURLParams(GURL(chrome::kAutofillHelpURL),
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             content::PAGE_TRANSITION_LINK,
                             false));
  return false;
}

