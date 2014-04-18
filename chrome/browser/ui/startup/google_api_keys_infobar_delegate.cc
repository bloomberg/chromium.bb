// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/google_api_keys_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"


// static
void GoogleApiKeysInfoBarDelegate::Create(InfoBarService* infobar_service) {
  if (google_apis::HasKeysConfigured())
    return;

  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new GoogleApiKeysInfoBarDelegate())));
}

GoogleApiKeysInfoBarDelegate::GoogleApiKeysInfoBarDelegate()
    : ConfirmInfoBarDelegate() {
}

GoogleApiKeysInfoBarDelegate::~GoogleApiKeysInfoBarDelegate() {
}

base::string16 GoogleApiKeysInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MISSING_GOOGLE_API_KEYS);
}

int GoogleApiKeysInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

base::string16 GoogleApiKeysInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool GoogleApiKeysInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
          GURL("http://www.chromium.org/developers/how-tos/api-keys"),
          content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          content::PAGE_TRANSITION_LINK, false));
  return false;
}
