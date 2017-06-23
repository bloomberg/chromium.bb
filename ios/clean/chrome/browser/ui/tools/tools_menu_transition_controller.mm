// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/tools_menu_transition_controller.h"

#import "ios/clean/chrome/browser/ui/presenters/menu_presentation_controller.h"

@interface ToolsMenuTransitionController ()
@property(nonatomic, weak) id<ToolsMenuCommands> dispatcher;
@end

@implementation ToolsMenuTransitionController
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
  menuPresentation.dispatcher = static_cast<id>(self.dispatcher);
  return menuPresentation;
}

@end
