// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridViewController
@synthesize delegate = _delegate;
@synthesize animationDelegate = _animationDelegate;
@synthesize dispatcher = _dispatcher;
@synthesize transitionContext = _transitionContext;

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor whiteColor];
}

#pragma mark - TabSwitcher

- (void)restoreInternalStateWithMainTabModel:(TabModel*)mainModel
                                 otrTabModel:(TabModel*)otrModel
                              activeTabModel:(TabModel*)activeModel {
}

- (UIViewController*)viewController {
  return self;
}

- (void)prepareForDisplayAtSize:(CGSize)size {
}

- (void)showWithSelectedTabAnimation {
}

- (Tab*)dismissWithNewTabAnimationToModel:(TabModel*)targetModel
                                  withURL:(const GURL&)url
                                  atIndex:(NSUInteger)position
                               transition:(ui::PageTransition)transition {
  return nil;
}

- (void)setOtrTabModel:(TabModel*)otrModel {
}

- (void)tabSwitcherDismissWithModel:(TabModel*)model animated:(BOOL)animated {
}

@end
