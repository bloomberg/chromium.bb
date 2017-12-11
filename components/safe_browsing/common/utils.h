// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Safe Browsing utility functions.

#ifndef COMPONENTS_SAFE_BROWSING_COMMON_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_COMMON_UTILS_H_

#include "base/time/time.h"
#include "url/gurl.h"

namespace safe_browsing {

// Shorten URL by replacing its contents with its SHA256 hash if it has data
// scheme.
std::string ShortURLForReporting(const GURL& url);

// UMA histogram helper for logging "SB2.NoUserActionResourceLoadingDelay".
// Logs the total delay caused by SafeBrowsing for a resource load, if the
// SafeBrowsing interstitial page is not showed. At most one value is reported
// for each resource load. If SafeBrowsing causes delays at different stages of
// a load, the sum of all the delays will be reported.
void LogNoUserActionResourceLoadingDelay(base::TimeDelta time);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_COMMON_UTILS_H_
