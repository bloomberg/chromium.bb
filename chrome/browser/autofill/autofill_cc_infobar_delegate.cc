// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"

#include "app/l10n_util.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/autofill/autofill_cc_infobar.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"

AutoFillCCInfoBarDelegate::AutoFillCCInfoBarDelegate(TabContents* tab_contents,
                                                     AutoFillManager* host)
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

string16 AutoFillCCInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_INFOBAR_TEXT);
}

SkBitmap* AutoFillCCInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_AUTOFILL);
}

int AutoFillCCInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 AutoFillCCInfoBarDelegate::GetButtonLabel(
    ConfirmInfoBarDelegate::InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_INFOBAR_ACCEPT);
  else if (button == BUTTON_CANCEL)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_INFOBAR_DENY);
  else
    NOTREACHED();

  return string16();
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
  if (host_) {
    host_->OnInfoBarClosed(false);
    host_ = NULL;
  }
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

#if defined(OS_WIN)
InfoBar* AutoFillCCInfoBarDelegate::CreateInfoBar() {
  return CreateAutofillCcInfoBar(this);
}
#endif  // defined(OS_WIN)

InfoBarDelegate::Type AutoFillCCInfoBarDelegate::GetInfoBarType() {
  return PAGE_ACTION_TYPE;
}
