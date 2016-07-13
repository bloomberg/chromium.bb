// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_PREF_NAMES_H_
#define COMPONENTS_NTP_TILES_PREF_NAMES_H_

namespace ntp_tiles {
namespace prefs {

// TODO(treib): Remove after M55.
extern const char kDeprecatedNTPSuggestionsURL[];
extern const char kDeprecatedNTPSuggestionsIsPersonal[];

extern const char kNumPersonalSuggestions[];

extern const char kPopularSitesOverrideURL[];
extern const char kPopularSitesOverrideCountry[];
extern const char kPopularSitesOverrideVersion[];

}  // namespace prefs
}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_PREF_NAMES_H_
