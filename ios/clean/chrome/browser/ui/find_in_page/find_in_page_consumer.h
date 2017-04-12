// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_FIND_IN_PAGE_FIND_IN_PAGE_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_FIND_IN_PAGE_FIND_IN_PAGE_CONSUMER_H_

// FindInPageConsumer sets the current appearance of the find bar based on
// various sources provided by the mediator.
@protocol FindInPageConsumer

// Updates the display of the total number of matches on the page and the index
// of the currently-highlighted matches.
- (void)setCurrentMatch:(NSUInteger)current ofTotalMatches:(NSUInteger)total;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_FIND_IN_PAGE_FIND_IN_PAGE_CONSUMER_H_
