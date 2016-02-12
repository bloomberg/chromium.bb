// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_UTILS_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_UTILS_H_

#include "components/suggestions/suggestions_utils.h"

class Profile;

namespace suggestions {

// Returns the current SyncState for use with the SuggestionsService.
SyncState GetSyncState(Profile* profile);

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_SUGGESTIONS_UTILS_H_
