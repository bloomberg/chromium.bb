// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRNET_CONSUMER_APP_DELEGATE_
#define CRNET_CONSUMER_APP_DELEGATE_

#import <UIKit/UIKit.h>

@class CrNetConsumerViewController;

// The main app controller and UIApplicationDelegate.
@interface CrNetConsumerAppDelegate : UIResponder <UIApplicationDelegate>

@property(strong, nonatomic) UIWindow* window;
@property(strong, nonatomic) CrNetConsumerViewController* viewController;

@end

#endif  // CRNET_CONSUMER_APP_DELEGATE_
