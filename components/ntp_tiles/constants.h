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

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_CONSTANTS_H_
