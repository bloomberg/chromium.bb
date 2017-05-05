// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/switches.h"

namespace ntp_tiles {
namespace switches {

// Enables using the default search engine country to show country specific
// popular sites on the NTP.
const char kEnableNTPSearchEngineCountryDetection[] =
    "enable-ntp-search-engine-country-detection";

// Enables showing popular sites on the NTP.
const char kEnableNTPPopularSites[] = "enable-ntp-popular-sites";

// Disables showing popular sites on the NTP.
const char kDisableNTPPopularSites[] = "disable-ntp-popular-sites";

// Enables the new Google favicon server for fetching favicons for Most Likely
// tiles on the New Tab Page.
const char kEnableNtpMostLikelyFaviconsFromServer[] =
    "enable-ntp-most-likely-favicons-from-server";

// Disables the new Google favicon server for fetching favicons for Most Likely
// tiles on the New Tab Page.
const char kDisableNtpMostLikelyFaviconsFromServer[] =
    "disable-ntp-most-likely-favicons-from-server";

}  // namespace switches
}  // namespace ntp_tiles
