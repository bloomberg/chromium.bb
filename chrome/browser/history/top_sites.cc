// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites.h"

#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/browser/history/top_sites_impl.h"
#include "chrome/browser/history/top_sites_likely_impl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

namespace history {

namespace {

// Constants for the most visited tile placement field trial.
// ex:
// "OneEightGroup_Flipped" --> Will cause tile 1 and 8 to be flipped.
// "OneEightGroup_NoChange" --> Will not flip anything.
//
// See field trial config (MostVisitedTilePlacement.json) for details.
const char kMostVisitedFieldTrialName[] = "MostVisitedTilePlacement";
const char kOneEightGroupPrefix[] = "OneEight";
const char kOneFourGroupPrefix[] = "OneFour";
const char kFlippedSuffix[] = "Flipped";

}  // namespace

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
  if (base::FieldTrialList::FindFullName("MostLikely") == "Likely_Client") {
    // Experimental group. Enabled through a command-line flag.
    TopSitesLikelyImpl* top_sites_likely_impl = new TopSitesLikelyImpl(profile);
    top_sites_likely_impl->Init(db_name);
    return top_sites_likely_impl;
  }
  TopSitesImpl* top_sites_impl = new TopSitesImpl(profile);
  top_sites_impl->Init(db_name);
  return top_sites_impl;
}

// static
void TopSites::MaybeShuffle(MostVisitedURLList* data) {
  const std::string group_name =
      base::FieldTrialList::FindFullName(kMostVisitedFieldTrialName);

  // Depending on the study group of the client, we might flip the 1st and 4th
  // tiles, or the 1st and 8th, or do nothing.
  if (EndsWith(group_name, kFlippedSuffix, true)) {
    size_t index_to_flip = 0;
    if (StartsWithASCII(group_name, kOneEightGroupPrefix, true) &&
        data->size() >= 8) {
      index_to_flip = 7;
    } else if (StartsWithASCII(group_name, kOneFourGroupPrefix, true) &&
               data->size() >= 4) {
      index_to_flip = 3;
    }

    if (data->empty() || (*data)[index_to_flip].url.is_empty())
      return;
    std::swap((*data)[0], (*data)[index_to_flip]);
  }
}

}  // namespace history
