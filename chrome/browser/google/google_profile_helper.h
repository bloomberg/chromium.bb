// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides Google-related information for a given Profile.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_PROFILE_HELPER_H__
#define CHROME_BROWSER_GOOGLE_GOOGLE_PROFILE_HELPER_H__

#include <string>

#include "base/basictypes.h"

class GURL;
class Profile;

namespace google_profile_helper {

// Returns the current Google homepage URL. Guaranteed to synchronously return
// a value at all times (even during startup, in unittest mode or if |profile|
// is NULL).
GURL GetGoogleHomePageURL(Profile* profile);

}  // namespace google_profile_helper

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_PROFILE_HELPER_H__
