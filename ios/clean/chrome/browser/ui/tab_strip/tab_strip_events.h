// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_EVENTS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_EVENTS_H_

#import <Foundation/Foundation.h>

// PLACEHOLDER: TabStrip related events, this might change or be renamed once we
// figure out the command architecture.
@protocol TabStripEvents
@optional
// Event triggered when the TabStrip has been hidden.
- (void)tabStripDidHide:(id)sender;
// Event triggered when the TabStrip has been shown.
- (void)tabStripDidShow:(id)sender;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_EVENTS_H_
