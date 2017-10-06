// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/transitions/zooming_menu_transition_controller.h"

#import "ios/clean/chrome/browser/ui/transitions/presenters/menu_presentation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ZoomingMenuTransitionController ()
@property(nonatomic, weak) id<ToolsMenuCommands> dispatcher;
@end

@implementation ZoomingMenuTransitionController
@synthesize dispatcher = _dispatcher;

- (instancetype)initWithDispatcher:(id<ToolsMenuCommands>)dispatcher {
  self = [super init];
  if (self) {
    _dispatcher = dispatcher;
  }
  return self;
}

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  MenuPresentationController* menuPresentation =
      [[MenuPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  menuPresentation.dispatcher = self.dispatcher;
  return menuPresentation;
}

@end
