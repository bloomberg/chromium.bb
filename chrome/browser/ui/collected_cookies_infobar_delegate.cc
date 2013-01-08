// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"

#include "base/logging.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// static
void CollectedCookiesInfoBarDelegate::Create(InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new CollectedCookiesInfoBarDelegate(infobar_service)));
}

CollectedCookiesInfoBarDelegate::CollectedCookiesInfoBarDelegate(
    InfoBarService* infobar_service)
    : ConfirmInfoBarDelegate(infobar_service) {
}

gfx::Image* CollectedCookiesInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_COOKIE);
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

string16 CollectedCookiesInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_INFOBAR_BUTTON);
}

bool CollectedCookiesInfoBarDelegate::Accept() {
  owner()->GetWebContents()->GetController().Reload(true);
  return true;
}
