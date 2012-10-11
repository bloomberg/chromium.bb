// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_RATINGS_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_RATINGS_VIEW_H_

#import <Cocoa/Cocoa.h>

// This view displays a 0 to 5 star ratting.
@interface RatingsView2 : NSView

- (id)initWithRating:(double)rating;

@end

#endif  // CHROME_BROWSER_UI_COCOA_RATINGS_VIEW_H_
