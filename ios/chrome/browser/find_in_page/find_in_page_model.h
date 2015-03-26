// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_MODEL_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_MODEL_H_

#import <UIKit/UIKit.h>
#include "base/mac/scoped_nsobject.h"

// This is a simplified version of find_tab_helper.cc.
@interface FindInPageModel : NSObject {
 @private
  // Should find in page be displayed.
  BOOL enabled_;
  // The current search string.
  base::scoped_nsobject<NSString> text_;
  // The number of matches for |text_|
  NSUInteger matches_;
  // The currently higlighted index.
  NSUInteger currentIndex_;
  // The content offset needed to display the |currentIndex_| match.
  CGPoint currentPoint_;
}

@property(nonatomic, readwrite, assign) BOOL enabled;
@property(nonatomic, readonly) NSString* text;
@property(nonatomic, readonly) NSUInteger matches;
@property(nonatomic, readonly) NSUInteger currentIndex;
@property(nonatomic, readonly) CGPoint currentPoint;

// Update the query string and the number of matches.
- (void)updateQuery:(NSString*)query matches:(NSUInteger)matches;
// Update the current match index and its found position.
- (void)updateIndex:(NSInteger)index atPoint:(CGPoint)point;

@end

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_MODEL_H_
