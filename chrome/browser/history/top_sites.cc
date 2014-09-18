// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites.h"

#include "base/strings/string_util.h"
#include "chrome/browser/history/top_sites_impl.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "grit/theme_resources.h"

namespace history {

const TopSites::PrepopulatedPage kPrepopulatedPages[] = {
#if defined(OS_ANDROID)
    { IDS_MOBILE_WELCOME_URL, IDS_NEW_TAB_CHROME_WELCOME_PAGE_TITLE,
    IDR_PRODUCT_LOGO_16, IDR_NEWTAB_CHROME_WELCOME_PAGE_THUMBNAIL,
    SkColorSetRGB(0, 147, 60) }
#else
  { IDS_CHROME_WELCOME_URL, IDS_NEW_TAB_CHROME_WELCOME_PAGE_TITLE,
    IDR_PRODUCT_LOGO_16, IDR_NEWTAB_CHROME_WELCOME_PAGE_THUMBNAIL,
    SkColorSetRGB(0, 147, 60) },
#endif
#if !defined(OS_ANDROID)
  { IDS_WEBSTORE_URL, IDS_EXTENSION_WEB_STORE_TITLE,
    IDR_WEBSTORE_ICON_16, IDR_NEWTAB_WEBSTORE_THUMBNAIL,
    SkColorSetRGB(63, 132, 197) }
#endif
};

// static
TopSites* TopSites::Create(Profile* profile, const base::FilePath& db_name) {
  TopSitesImpl* top_sites_impl = new TopSitesImpl(profile);
  top_sites_impl->Init(db_name);
  return top_sites_impl;
}

TopSites::TopSites() {
}

TopSites::~TopSites() {
}

void TopSites::AddObserver(TopSitesObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TopSites::RemoveObserver(TopSitesObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void TopSites::NotifyTopSitesLoaded() {
  FOR_EACH_OBSERVER(TopSitesObserver, observer_list_, TopSitesLoaded(this));
}

void TopSites::NotifyTopSitesChanged() {
  FOR_EACH_OBSERVER(TopSitesObserver, observer_list_, TopSitesChanged(this));
}

}  // namespace history
