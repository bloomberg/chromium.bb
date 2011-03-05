// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/browser_list.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

AutoFillCCInfoBarDelegate::AutoFillCCInfoBarDelegate(TabContents* tab_contents,
                                                     AutofillManager* host)
    : ConfirmInfoBarDelegate(tab_contents),
      host_(host) {
}

AutoFillCCInfoBarDelegate::~AutoFillCCInfoBarDelegate() {
}

bool AutoFillCCInfoBarDelegate::ShouldExpire(
    const NavigationController::LoadCommittedDetails& details) const {
  // The user has submitted a form, causing the page to navigate elsewhere. We
  // don't want the infobar to be expired at this point, because the user won't
  // get a chance to answer the question.
  return false;
}

void AutoFillCCInfoBarDelegate::InfoBarClosed() {
  if (host_) {
    host_->OnInfoBarClosed(false);
    host_ = NULL;
  }
  delete this;
}

SkBitmap* AutoFillCCInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_AUTOFILL);
}

InfoBarDelegate::Type AutoFillCCInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 AutoFillCCInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_INFOBAR_TEXT);
}

string16 AutoFillCCInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_AUTOFILL_CC_INFOBAR_ACCEPT : IDS_AUTOFILL_CC_INFOBAR_DENY);
}

bool AutoFillCCInfoBarDelegate::Accept() {
  UMA_HISTOGRAM_COUNTS("AutoFill.CCInfoBarAccepted", 1);
  if (host_) {
    host_->OnInfoBarClosed(true);
    host_ = NULL;
  }
  return true;
}

bool AutoFillCCInfoBarDelegate::Cancel() {
  UMA_HISTOGRAM_COUNTS("AutoFill.CCInfoBarDenied", 1);
  return true;
}

string16 AutoFillCCInfoBarDelegate::GetLinkText() {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_LEARN_MORE);
}

bool AutoFillCCInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->OpenAutoFillHelpTabAndActivate();
  return false;
}
