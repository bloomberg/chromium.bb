// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_CONSTANTS_H_
#define COMPONENTS_NTP_TILES_CONSTANTS_H_

namespace base {
struct Feature;
}  // namespace base

namespace ntp_tiles {

// Name of the field trial to configure PopularSites.
extern const char kPopularSitesFieldTrialName[];

// This feature is enabled by default. Otherwise, users who need it would not
// get the right configuration timely enough. The configuration affects only
// Android or iOS users.
extern const base::Feature kPopularSitesBakedInContentFeature;

// Feature to allow the new Google favicon server for fetching favicons for Most
// Likely tiles on the New Tab Page.
extern const base::Feature kNtpMostLikelyFaviconsFromServerFeature;

// Feature to provide site exploration tiles in addition to personal tiles.
extern const base::Feature kSiteExplorationUiFeature;

// If this feature is enabled, we enable popular sites in the suggestions UI.
extern const base::Feature kUsePopularSitesSuggestions;

// Feature that enables the GM2 design for Most Visited. Desktop only.
extern const base::Feature kNtpIcons;

// Feature that enables custom links and replaces Most Visited. Implicitly
// enables |kNtpIcons|. Desktop only.
extern const base::Feature kNtpCustomLinks;

// Returns whether the GM2 design for Most Visited is enabled.
bool IsMDIconsEnabled();

// Returns whether the custom links is enabled.
bool IsCustomLinksEnabled();

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_CONSTANTS_H_
