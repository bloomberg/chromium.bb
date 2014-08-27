// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"

#include "base/logging.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"


// static
void CollectedCookiesInfoBarDelegate::Create(InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new CollectedCookiesInfoBarDelegate())));
}

CollectedCookiesInfoBarDelegate::CollectedCookiesInfoBarDelegate()
    : ConfirmInfoBarDelegate() {
}

CollectedCookiesInfoBarDelegate::~CollectedCookiesInfoBarDelegate() {
}

int CollectedCookiesInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_COOKIE;
}

infobars::InfoBarDelegate::Type
CollectedCookiesInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

base::string16 CollectedCookiesInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_INFOBAR_MESSAGE);
}

int CollectedCookiesInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 CollectedCookiesInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_INFOBAR_BUTTON);
}

bool CollectedCookiesInfoBarDelegate::Accept() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  web_contents->GetController().Reload(true);
  return true;
}
