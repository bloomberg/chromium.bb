// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmark_new_generation_features.h"

// Feature flag for the new bookmark UI.
const base::Feature kBookmarkNewGeneration{"BookmarkNewGeneration",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Feature flag for the new edit page and folder picker of the new bookmark UI.
const base::Feature kBookmarkNewEditPage{"BookmarkNewEditPage",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
