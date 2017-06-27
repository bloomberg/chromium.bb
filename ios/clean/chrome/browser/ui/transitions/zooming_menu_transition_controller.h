// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ZOOMING_MENU_TRANSITION_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ZOOMING_MENU_TRANSITION_CONTROLLER_H_

#import "ios/clean/chrome/browser/ui/transitions/zoom_transition_controller.h"

@protocol ToolsMenuCommands;

// Transition delegate object for the ToolsMenuVC which inherits from
// ZoomTransitionController. It conforms to the
// UIViewControllerTransitioningDelegate protocol and provides the ToolsMenuVC
// with MenuPresentationController as a UIPresentationController. This object
// drives the animation and frame of the presented ToolsMenuVC.
@interface ZoomingMenuTransitionController : ZoomTransitionController

// A dispatcher is needed in order to close the presented ToolsMenuVC.
- (instancetype)initWithDispatcher:(id<ToolsMenuCommands>)dispatcher;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ZOOMING_MENU_TRANSITION_CONTROLLER_H_
