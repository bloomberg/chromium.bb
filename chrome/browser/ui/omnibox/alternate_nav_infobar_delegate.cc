// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"


// static
void AlternateNavInfoBarDelegate::Create(InfoBarService* infobar_service,
                                         const AutocompleteMatch& match) {
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new AlternateNavInfoBarDelegate(infobar_service, match)));
}

AlternateNavInfoBarDelegate::AlternateNavInfoBarDelegate(
    InfoBarService* owner,
    const AutocompleteMatch& match)
    : InfoBarDelegate(owner),
      match_(match) {
  DCHECK(match_.destination_url.is_valid());
}

AlternateNavInfoBarDelegate::~AlternateNavInfoBarDelegate() {
}

string16 AlternateNavInfoBarDelegate::GetMessageTextWithOffset(
    size_t* link_offset) const {
  const string16 label = l10n_util::GetStringFUTF16(
      IDS_ALTERNATE_NAV_URL_VIEW_LABEL, string16(), link_offset);
  return label;
}

string16 AlternateNavInfoBarDelegate::GetLinkText() const {
  return UTF8ToUTF16(match_.destination_url.spec());
}

bool AlternateNavInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  // Pretend the user typed this URL, so that navigating to it will be the
  // default action when it's typed again in the future.
  web_contents()->OpenURL(content::OpenURLParams(
      match_.destination_url, content::Referrer(), disposition,
      content::PAGE_TRANSITION_TYPED, false));

  // We should always close, even if the navigation did not occur within this
  // WebContents.
  return true;
}

int AlternateNavInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_ALT_NAV_URL;
}

InfoBarDelegate::Type AlternateNavInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}
