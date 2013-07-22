// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_SYNC_PROMO_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_SYNC_PROMO_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

class Browser;
@class HyperlinkTextView;

// Controller of the bookmark sync promo displayed at the bottom of the
// bookmark bubble.
@interface BookmarkSyncPromoController : NSViewController<NSTextViewDelegate> {
 @private
  // The browser in which the sign in page will be loaded.
  Browser* browser_;  // weak

  // The text view that displays the promo message. Ownership is shared between
  // the controller and its view.
  base::scoped_nsobject<HyperlinkTextView> textView_;
}

@property(nonatomic, readonly) CGFloat borderWidth;

- (id)initWithBrowser:(Browser*)browser;

// Preferred height of the sync promo view for a given width. The border is
// is included in the provided width and in the returned height.
- (CGFloat)preferredHeightForWidth:(CGFloat)width;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_SYNC_PROMO_CONTROLLER_H_
