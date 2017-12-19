// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_SNAPSHOTTING_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_TAB_SNAPSHOTTING_DELEGATE_H_

#import <UIKit/UIKit.h>

@class Tab;

// A protocol implemented by a snapshotting delegate of Tab.
@protocol TabSnapshottingDelegate

// Returns the edge insets used to crop the snapshot during generation. If the
// snapshot should not be cropped, then UIEdgeInsetsZero can be returned.
- (UIEdgeInsets)snapshotEdgeInsetsForTab:(Tab*)tab;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_SNAPSHOTTING_DELEGATE_H_
