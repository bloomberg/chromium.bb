// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_adaptor.h"

#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/main/view_controller_swapping.h"
#import "ios/web/public/navigation_manager.h"

#import "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridAdaptor
// TabSwitcher properties.
@synthesize delegate = _delegate;
@synthesize animationDelegate = _animationDelegate;
// Public properties
@synthesize tabGridViewController = _tabGridViewController;
@synthesize adaptedDispatcher = _adaptedDispatcher;
@synthesize incognitoMediator = _incognitoMediator;

#pragma mark - TabSwitcher

- (id<ApplicationCommands, BrowserCommands, OmniboxFocuser, ToolbarCommands>)
    dispatcher {
  return static_cast<id<ApplicationCommands, BrowserCommands, OmniboxFocuser,
                        ToolbarCommands>>(self.adaptedDispatcher);
}

- (void)setAnimationDelegate:
    (id<TabSwitcherAnimationDelegate>)animationDelegate {
  NOTREACHED()
      << "The tab grid shouldn't need a tab switcher animation delegate.";
  _animationDelegate = nil;
}

- (void)restoreInternalStateWithMainTabModel:(TabModel*)mainModel
                                 otrTabModel:(TabModel*)otrModel
                              activeTabModel:(TabModel*)activeModel {
  // This is a no-op, but it will be called frequently.
}

- (void)prepareForDisplayAtSize:(CGSize)size {
  NOTREACHED();
}

- (void)showWithSelectedTabAnimation {
  NOTREACHED();
}

- (UIViewController*)viewController {
  return self.tabGridViewController;
}

- (Tab*)dismissWithNewTabAnimationToModel:(TabModel*)targetModel
                                  withURL:(const GURL&)URL
                                  atIndex:(NSUInteger)position
                               transition:(ui::PageTransition)transition {
  NSUInteger tabIndex = position;
  if (position > targetModel.count)
    tabIndex = targetModel.count;

  web::NavigationManager::WebLoadParams loadParams(URL);
  loadParams.transition_type = transition;

  // Create the new tab.
  Tab* tab = [targetModel insertTabWithLoadParams:loadParams
                                           opener:nil
                                      openedByDOM:NO
                                          atIndex:tabIndex
                                     inBackground:NO];

  // Tell the delegate to display the tab.
  [self.delegate tabSwitcher:self shouldFinishWithActiveModel:targetModel];

  return tab;
}

- (void)setOtrTabModel:(TabModel*)otrModel {
  self.incognitoMediator.tabModel = otrModel;
}

@end
