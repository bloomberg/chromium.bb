// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CRNET_CRNET_CONSUMER_CRNET_CONSUMER_APP_DELEGATE_H_
#define IOS_CRNET_CRNET_CONSUMER_CRNET_CONSUMER_APP_DELEGATE_H_

#import <UIKit/UIKit.h>

@class CrNetConsumerViewController;

// The main app controller and UIApplicationDelegate.
@interface CrNetConsumerAppDelegate : UIResponder <UIApplicationDelegate>

@property(strong, nonatomic) UIWindow* window;
@property(strong, nonatomic) CrNetConsumerViewController* viewController;

@end

#endif  // IOS_CRNET_CRNET_CONSUMER_CRNET_CONSUMER_APP_DELEGATE_H_
