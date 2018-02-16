// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_adaptor.h"

#import "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridAdaptor
// TabSwitcher properties.
@synthesize delegate = _delegate;
@synthesize animationDelegate = _animationDelegate;
@synthesize dispatcher = _dispatcher;
// Public properties
@synthesize tabGridViewController = _tabGridViewController;

#pragma mark - TabSwitcher

- (void)restoreInternalStateWithMainTabModel:(TabModel*)mainModel
                                 otrTabModel:(TabModel*)otrModel
                              activeTabModel:(TabModel*)activeModel {
  // No-op.
}

- (void)prepareForDisplayAtSize:(CGSize)size {
  // No-op.
}

- (void)showWithSelectedTabAnimation {
  // No-op.
}

- (UIViewController*)viewController {
  return self.tabGridViewController;
}

// Create a new tab in |targetModel| with the url |url| at |position|, using
// page transition |transition|. Implementors are expected to also
// perform an animation from the selected tab in the tab switcher to the
// newly created tab in the content area. Objects adopting this protocol should
// call the following delegate methods:
//   |-tabSwitcher:dismissTransitionWillStartWithActiveModel:|
//   |-tabSwitcherDismissTransitionDidEnd:|
// to inform the delegate when this animation begins and ends.
- (Tab*)dismissWithNewTabAnimationToModel:(TabModel*)targetModel
                                  withURL:(const GURL&)url
                                  atIndex:(NSUInteger)position
                               transition:(ui::PageTransition)transition {
  return nil;
}

// Updates the OTR (Off The Record) tab model. Should only be called when both
// the current OTR tab model and the new OTR tab model are either nil or contain
// no tabs. This must be called after the otr tab model has been deleted because
// the incognito browser state is deleted.
- (void)setOtrTabModel:(TabModel*)otrModel {
  // No-op
}

@end
