// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/modal_ntp.h"

#include "ios/chrome/browser/bookmarks/bookmark_new_generation_features.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BOOL PresentNTPPanelModally() {
  return !IsIPadIdiom() || base::FeatureList::IsEnabled(kBookmarkNewGeneration);
}

BOOL IsBookmarksHostEnabled() {
  return !PresentNTPPanelModally();
}
