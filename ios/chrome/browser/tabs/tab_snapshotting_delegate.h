// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_SNAPSHOTTING_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_TAB_SNAPSHOTTING_DELEGATE_H_

#import <UIKit/UIKit.h>

@class Tab;

// A protocol implemented by a snapshotting delegate of Tab.
@protocol TabSnapshottingDelegate

// Returns the rect (in the |tab.webController.view|'s coordinate space) that is
// used to render the page's content.
- (CGRect)snapshotContentAreaForTab:(Tab*)tab;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_SNAPSHOTTING_DELEGATE_H_
