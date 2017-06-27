// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ROOT_ROOT_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ROOT_ROOT_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/transitions/animators/zoom_transition_delegate.h"
#import "ios/clean/chrome/browser/ui/transitions/presenters/menu_presentation_delegate.h"

// View controller that wholly contains a content view controller.
@interface RootContainerViewController
    : UIViewController<MenuPresentationDelegate, ZoomTransitionDelegate>

// View controller showing the main content.
@property(nonatomic, strong) UIViewController* contentViewController;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ROOT_ROOT_CONTAINER_VIEW_CONTROLLER_H_
