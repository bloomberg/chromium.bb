// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_profile_helper.h"

#include "chrome/browser/google/google_url_tracker_factory.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/google/core/browser/google_util.h"
#include "url/gurl.h"

namespace google_profile_helper {

GURL GetGoogleHomePageURL(Profile* profile) {
  const GoogleURLTracker* tracker =
      GoogleURLTrackerFactory::GetForProfile(profile);
  return tracker ?
      tracker->google_url() : GURL(GoogleURLTracker::kDefaultGoogleHomepage);
}

}  // namespace google_profile_helper
