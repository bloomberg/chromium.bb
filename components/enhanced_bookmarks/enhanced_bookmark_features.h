// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_FEATURES_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_FEATURES_H_

#include "build/build_config.h"

#if defined(OS_IOS) || defined(OS_ANDROID)

namespace enhanced_bookmarks {

// Returns true if enhanced bookmarks is enabled.
bool IsEnhancedBookmarksEnabled();

}  // namespace enhanced_bookmarks

#endif

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_FEATURES_H_
