// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

class WebStateList;

// Coordinator responsible for presenting and dismissing the translate infobar's
// language selection popup menu, translate options popup menu, and translate
// options notifications.
@interface TranslateInfobarCoordinator : ChromeCoordinator

// Creates a coordinator that uses |viewController|, |browserState|, and
// |webStateList|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList;

// Unavailable, use -initWithBaseViewController:browserState:webStateList:.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_INFOBAR_COORDINATOR_H_
