// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_H_

#include "base/basictypes.h"

class Profile;

namespace chrome {
namespace search {

// Returns whether the Instant extended API is enabled for the given |profile|.
// |profile| may not be NULL.
bool IsInstantExtendedAPIEnabled(const Profile* profile);

// Returns the value to pass to the &espv cgi parameter when loading the
// embedded search page from the user's default search provider.  Will be
// 0 if the Instant Extended API is not enabled.
uint64 EmbeddedSearchPageVersion(const Profile* profile);

// Force the instant extended API to be enabled for tests.
void EnableInstantExtendedAPIForTesting();

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_H_
