// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_geolocation_disclosure_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/infobars/search_geolocation_disclosure_infobar.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

SearchGeolocationDisclosureInfoBarDelegate::
    ~SearchGeolocationDisclosureInfoBarDelegate() {}

// static
void SearchGeolocationDisclosureInfoBarDelegate::Create(
    content::WebContents* web_contents, const GURL& search_url) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  // Add the new delegate.
  infobar_service->AddInfoBar(
      base::MakeUnique<SearchGeolocationDisclosureInfoBar>(
          base::WrapUnique(new SearchGeolocationDisclosureInfoBarDelegate(
              web_contents, search_url))));
}

// static
bool SearchGeolocationDisclosureInfoBarDelegate::
    IsSearchGeolocationDisclosureOpen(content::WebContents* web_contents) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* existing_infobar = infobar_service->infobar_at(i);
    if (existing_infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::
            SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE) {
      return true;
    }
  }

  return false;
}

SearchGeolocationDisclosureInfoBarDelegate::
    SearchGeolocationDisclosureInfoBarDelegate(
        content::WebContents* web_contents,
        const GURL& search_url)
    : infobars::InfoBarDelegate(), search_url_(search_url) {
  pref_service_ = Profile::FromBrowserContext(web_contents->GetBrowserContext())
                      ->GetPrefs();
  base::string16 link = l10n_util::GetStringUTF16(
      IDS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_SETTINGS_LINK_TEXT);
  size_t offset;
  message_text_ = l10n_util::GetStringFUTF16(
      IDS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_TEXT, link, &offset);
  inline_link_range_ = gfx::Range(offset, offset + link.length());
}

void SearchGeolocationDisclosureInfoBarDelegate::InfoBarDismissed() {
  pref_service_->SetBoolean(prefs::kSearchGeolocationDisclosureDismissed, true);
}

infobars::InfoBarDelegate::Type
SearchGeolocationDisclosureInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

infobars::InfoBarDelegate::InfoBarIdentifier
SearchGeolocationDisclosureInfoBarDelegate::GetIdentifier() const {
  return SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE;
}

int SearchGeolocationDisclosureInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_GEOLOCATION;
}
