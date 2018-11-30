// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_SIDE_SWIPE_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_SIDE_SWIPE_PROVIDER_H_

#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"

class ReadingListModel;

@interface ReadingListSideSwipeProvider : NSObject<SideSwipeContentProvider>
- (instancetype)initWithReadingList:(ReadingListModel*)readingListModel;
@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_SIDE_SWIPE_PROVIDER_H_
