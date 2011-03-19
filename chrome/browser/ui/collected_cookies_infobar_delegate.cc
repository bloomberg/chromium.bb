// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"

#include "base/logging.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

CollectedCookiesInfoBarDelegate::CollectedCookiesInfoBarDelegate(
    TabContents* tab_contents)
    : ConfirmInfoBarDelegate(tab_contents),
      tab_contents_(tab_contents) {
}

SkBitmap* CollectedCookiesInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_INFOBAR_COOKIE);
}

InfoBarDelegate::Type CollectedCookiesInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 CollectedCookiesInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_INFOBAR_MESSAGE);
}

int CollectedCookiesInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 CollectedCookiesInfoBarDelegate::GetButtonLabel(InfoBarButton button)
    const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_INFOBAR_BUTTON);
}

bool CollectedCookiesInfoBarDelegate::Accept() {
  tab_contents_->controller().Reload(true);
  return true;
}
