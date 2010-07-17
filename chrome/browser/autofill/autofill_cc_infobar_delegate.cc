// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/autofill/autofill_cc_infobar.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

AutoFillCCInfoBarDelegate::AutoFillCCInfoBarDelegate(TabContents* tab_contents,
                                                     AutoFillManager* host)
    : ConfirmInfoBarDelegate(tab_contents),
      browser_(NULL),
      host_(host) {
  if (tab_contents) {
    // This is NULL for TestTabContents.
    if (tab_contents->delegate())
      browser_ = tab_contents->delegate()->GetBrowser();

    tab_contents->AddInfoBar(this);
  }
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

  // This will delete us.
  ConfirmInfoBarDelegate::InfoBarClosed();
}

std::wstring AutoFillCCInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetString(IDS_AUTOFILL_CC_INFOBAR_TEXT);
}

SkBitmap* AutoFillCCInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_AUTOFILL);
}

int AutoFillCCInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::wstring AutoFillCCInfoBarDelegate::GetButtonLabel(
    ConfirmInfoBarDelegate::InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetString(IDS_AUTOFILL_CC_INFOBAR_ACCEPT);
  else if (button == BUTTON_CANCEL)
    return l10n_util::GetString(IDS_AUTOFILL_CC_INFOBAR_DENY);
  else
    NOTREACHED();

  return std::wstring();
}

bool AutoFillCCInfoBarDelegate::Accept() {
  if (host_) {
    host_->OnInfoBarClosed(true);
    host_ = NULL;
  }
  return true;
}

bool AutoFillCCInfoBarDelegate::Cancel() {
  if (host_) {
    host_->OnInfoBarClosed(false);
    host_ = NULL;
  }
  return true;
}

std::wstring AutoFillCCInfoBarDelegate::GetLinkText() {
  return l10n_util::GetString(IDS_AUTOFILL_CC_LEARN_MORE);
}

bool AutoFillCCInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  browser_->OpenURL(GURL(kAutoFillLearnMoreUrl), GURL(), NEW_FOREGROUND_TAB,
                    PageTransition::TYPED);
  return false;
}

#if defined(OS_WIN)
InfoBar* AutoFillCCInfoBarDelegate::CreateInfoBar() {
  return CreateAutofillCcInfoBar(this);
}
#endif  // defined(OS_WIN)

