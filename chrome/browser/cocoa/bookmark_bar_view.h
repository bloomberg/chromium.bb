// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple custom NSView for the bookmark bar used to prevent clicking and
// dragging from moving the browser window.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_VIEW_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_VIEW_H_

#import <Cocoa/Cocoa.h>

@class BookmarkBarController;

@interface BookmarkBarView : NSView {
 @private
  BOOL dropIndicatorShown_;
  CGFloat dropIndicatorPosition_;

  IBOutlet BookmarkBarController* controller_;
  IBOutlet NSTextField* noItemTextfield_;
}
- (NSTextField*)noItemTextfield;
@end

@interface BookmarkBarView(TestingAPI)
- (void)setController:(id)controller;
@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_VIEW_H_
