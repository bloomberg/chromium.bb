// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/settings/settings_coordinator.h"

#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"
#import "ios/clean/chrome/browser/ui/settings/settings_main_page_coordinator.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Traverses the coordinator hierarchy in a pre-order depth-first traversal.
// |block| is called on all children of |coordinator|.
void TraverseCoordinatorHierarchy(BrowserCoordinator* coordinator,
                                  void (^block)(BrowserCoordinator*)) {
  for (BrowserCoordinator* child in coordinator.children) {
    block(child);
    TraverseCoordinatorHierarchy(child, block);
  }
}
}  // namespace

@interface SettingsCoordinator ()<SettingsNavigationControllerDelegate,
                                  UINavigationControllerDelegate>
@property(nonatomic, strong) SettingsNavigationController* viewController;
@end

@implementation SettingsCoordinator
@synthesize viewController = _viewController;

#pragma mark - BrowserCoordinator

- (void)start {
  DCHECK(!self.browser->browser_state()->IsOffTheRecord());
  SettingsMainPageCoordinator* mainPageCoordinator =
      [[SettingsMainPageCoordinator alloc] init];
  [self addChildCoordinator:mainPageCoordinator];
  [mainPageCoordinator start];
  self.viewController = [[SettingsNavigationController alloc]
      initWithRootViewController:mainPageCoordinator.viewController
                    browserState:self.browser->browser_state()
                        delegate:self];
  self.viewController.delegate = self;
  mainPageCoordinator.viewController.navigationItem.rightBarButtonItem =
      [self.viewController doneButton];
  [super start];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettingsAndOpenUrl:(OpenUrlCommand*)command {
  // Placeholder implementation to conform to the delegate protocol;
  // for now this just closes the settings without opening a URL.
  [self closeSettings];
}

- (void)closeSettingsAndOpenNewIncognitoTab {
  // Placeholder implementation to conform to the delegate protocol;
  // for now this just closes the settings without opening a new tab.
  [self closeSettings];
}

- (void)closeSettings {
  [static_cast<id>(self.browser->dispatcher()) closeSettings];
}

- (id)dispatcherForSettings {
  return nil;
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  UIViewController* fromViewController =
      [navigationController.transitionCoordinator
          viewControllerForKey:UITransitionContextFromViewControllerKey];
  if ([navigationController.viewControllers
          containsObject:fromViewController]) {
    return;
  }
  // Clean the coordinator hierarchy from coordinators whose view controller was
  // popped via the UI (system back button or swipe to go back.)
  TraverseCoordinatorHierarchy(self, ^(BrowserCoordinator* child) {
    if ([child.viewController isEqual:fromViewController]) {
      [child stop];
    }
  });
}

@end
