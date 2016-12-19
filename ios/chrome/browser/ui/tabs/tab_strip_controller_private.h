// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_PRIVATE_H_

#import <Foundation/Foundation.h>

@class Tab;
@class TabView;

// Exposed private methods for testing purpose.
@interface TabStripController ()
- (NSUInteger)indexForModelIndex:(NSUInteger)modelIndex;
@end

@interface TabStripController (Testing)
- (TabView*)existingTabViewForTab:(Tab*)tab;
@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_PRIVATE_H_
