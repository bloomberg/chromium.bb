// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_infobar_delegate.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

AutoFillInfoBarDelegate::AutoFillInfoBarDelegate(TabContents* tab_contents,
                                                 AutoFillManager* host)
    : ConfirmInfoBarDelegate(tab_contents),
      host_(host) {
  if (tab_contents)
    tab_contents->AddInfoBar(this);
}

AutoFillInfoBarDelegate::~AutoFillInfoBarDelegate() {
}

bool AutoFillInfoBarDelegate::ShouldExpire(
    const NavigationController::LoadCommittedDetails& details) const {
  // The user has submitted a form, causing the page to navigate elsewhere.  We
  // don't want the infobar to be expired at this point, because the user won't
  // get a chance to answer the question.
  return false;
}

void AutoFillInfoBarDelegate::InfoBarClosed() {
  Cancel();
  // This will delete us.
  ConfirmInfoBarDelegate::InfoBarClosed();
}

std::wstring AutoFillInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetString(IDS_AUTOFILL_INFOBAR_TEXT);
}

SkBitmap* AutoFillInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFOBAR_AUTOFILL);
}

int AutoFillInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::wstring AutoFillInfoBarDelegate::GetButtonLabel(
    ConfirmInfoBarDelegate::InfoBarButton button) const {
  if (button == BUTTON_OK)
    return l10n_util::GetString(IDS_AUTOFILL_INFOBAR_ACCEPT);

  return l10n_util::GetString(IDS_AUTOFILL_INFOBAR_DENY);
}

bool AutoFillInfoBarDelegate::Accept() {
  if (host_) {
    host_->SaveFormData();
    host_ = NULL;
  }
  return true;
}

bool AutoFillInfoBarDelegate::Cancel() {
  if (host_) {
    host_->Reset();
    host_ = NULL;
  }
  return true;
}
