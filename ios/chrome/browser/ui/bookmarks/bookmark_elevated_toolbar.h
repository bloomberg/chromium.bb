// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ELEVATED_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ELEVATED_TOOLBAR_H_

#import <CoreGraphics/CoreGraphics.h>

#import "ios/third_party/material_components_ios/src/components/ButtonBar/src/MaterialButtonBar.h"

@class MDCShadowLayer;

// A subclass of MDCButtonBar that has a Material shadow.
@interface BookmarksElevatedToolbar : MDCButtonBar

@property(nonatomic, readonly) MDCShadowLayer* shadowLayer;
@property(nonatomic, assign) CGFloat shadowElevation;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ELEVATED_TOOLBAR_H_
