// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_H_
#define IOS_SHARED_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_H_

#import <UIKit/UIKit.h>

// Protocol collecting all of the methods that broadcast keys will trigger
// in an observer. Each key maps to a specific observer method as indicated.
// (this mapping is generated in the implementation of the Broadcaster class).
//
// All of the methods in this protocol *must* return void and take exactly one
// argument.
@protocol ChromeBroadcastObserver<NSObject>
@optional

// Observer method for object that care about the current visibility of the tab
// strip.
- (void)broadcastTabStripVisible:(BOOL)visible;

// Observer method for objects that care about the current vertical (y-axis)
// scroll offset of the tab content area.
- (void)broadcastContentScrollOffset:(CGFloat)offset;

@end

#endif  // IOS_SHARED_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_H_
