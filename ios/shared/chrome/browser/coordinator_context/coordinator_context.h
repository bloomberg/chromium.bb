// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_COORDINATOR_CONTEXT_COORDINATOR_CONTEXT_H_
#define IOS_SHARED_CHROME_BROWSER_COORDINATOR_CONTEXT_COORDINATOR_CONTEXT_H_

#import <UIKit/UIKit.h>

@interface CoordinatorContext : NSObject

// Default is YES.
@property(nonatomic, assign, getter=isAnimated) BOOL animated;

@end

#endif  // IOS_SHARED_CHROME_BROWSER_COORDINATOR_CONTEXT_COORDINATOR_CONTEXT_H_
