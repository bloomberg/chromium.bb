// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/alternate_nav_infobar_delegate.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

AlternateNavInfoBarDelegate::AlternateNavInfoBarDelegate(
    InfoBarService* owner,
    const GURL& alternate_nav_url)
    : InfoBarDelegate(owner),
      alternate_nav_url_(alternate_nav_url) {
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
  return UTF8ToUTF16(alternate_nav_url_.spec());
}

bool AlternateNavInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  content::OpenURLParams params(
      alternate_nav_url_, content::Referrer(), disposition,
      // Pretend the user typed this URL, so that navigating to
      // it will be the default action when it's typed again in
      // the future.
      content::PAGE_TRANSITION_TYPED,
      false);
  owner()->GetWebContents()->OpenURL(params);

  // We should always close, even if the navigation did not occur within this
  // WebContents.
  return true;
}

AlternateNavInfoBarDelegate*
    AlternateNavInfoBarDelegate::AsAlternateNavInfoBarDelegate() {
  return this;
}

gfx::Image* AlternateNavInfoBarDelegate::GetIcon() const {
  return &ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_ALT_NAV_URL);
}

InfoBarDelegate::Type AlternateNavInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}
