// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_CONSUMER_H_

#import <Foundation/Foundation.h>

@class NewTabPageBarItem;

@protocol NTPConsumer

// Tells NTP what bar items to display.
- (void)setBarItems:(NSArray<NewTabPageBarItem*>*)items;

// Sets the item corresponding to the panel to be displayed when the NTP is
// loaded.
- (void)setFirstItemToDisplay:(NewTabPageBarItem*)itemToDisplay;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_CONSUMER_H_
