// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_COORDINATOR_CONTEXT_COORDINATOR_CONTEXT_H_
#define IOS_SHARED_CHROME_BROWSER_COORDINATOR_CONTEXT_COORDINATOR_CONTEXT_H_

#import <UIKit/UIKit.h>

// Holds data for the coordinator that uses it.
@interface CoordinatorContext : NSObject

// The view controller that the coordinator will use to present its content, if
// it is presenting content. This is not the view controller created and managed
// by the coordinator; it should be supplied by whatever object is creating
// the coordinator.
@property(nonatomic, weak) UIViewController* baseViewController;

// Default is YES.
@property(nonatomic, assign, getter=isAnimated) BOOL animated;

@end

#endif  // IOS_SHARED_CHROME_BROWSER_COORDINATOR_CONTEXT_COORDINATOR_CONTEXT_H_
