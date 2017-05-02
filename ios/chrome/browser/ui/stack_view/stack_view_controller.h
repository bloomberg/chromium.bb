// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_STACK_VIEW_STACK_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_STACK_VIEW_STACK_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher.h"
#include "ui/base/page_transition_types.h"

class GURL;
@class Tab;
@class TabModel;

// A protocol used by the StackViewController for test purposes.
@protocol StackViewControllerTestDelegate
@required
// Informs the delegate that the show tab animation starts.
- (void)stackViewControllerShowWithSelectedTabAnimationDidStart;
// Informs the delegate that the show tab animation finished.
- (void)stackViewControllerShowWithSelectedTabAnimationDidEnd;
// Informs the delegate that the preload of card views finished.
- (void)stackViewControllerPreloadCardViewsDidEnd;
@end

// Controller for the tab-switching UI displayed as a stack of tabs.
@interface StackViewController : UIViewController<TabSwitcher>

@property(nonatomic, assign) id<TabSwitcherDelegate> delegate;
@property(nonatomic, assign) id<StackViewControllerTestDelegate> testDelegate;

// Initializes with the given tab models, which must not be nil.
// |activeTabModel| is the model which starts active, and must be one of the
// other two models.
// TODO(stuartmorgan): Replace the word 'active' in these methods and the
// corresponding delegate methods. crbug.com/513782
- (instancetype)initWithMainTabModel:(TabModel*)mainModel
                         otrTabModel:(TabModel*)otrModel
                      activeTabModel:(TabModel*)activeModel;

// Restores internal state with the given tab models, which must not be nil.
// |activeTabModel| is the model which starts active, and must be one of the
// other two models. Should only be called when the object is not being shown.
- (void)restoreInternalStateWithMainTabModel:(TabModel*)mainModel
                                 otrTabModel:(TabModel*)otrModel
                              activeTabModel:(TabModel*)activeModel;

// Updates the otr tab model. Should only be called when both the current otr
// tab model and the new otr tab model are either nil or contain no tabs. Must
// be called when the otr tab model will be deleted because the incognito
// browser state is deleted.
- (void)setOtrTabModel:(TabModel*)otrModel;

// Performs an animation to zoom the selected tab from the full size of the
// content area to its proper place in the stack. Should be called after the
// view has been made visible.
- (void)showWithSelectedTabAnimation;

// Performs an animation to zoom the selected tab to the full size of the
// content area. When the animation completes, calls
// |-stackViewControllerDismissAnimationWillStartWithActiveModel:| and
// |-stackViewControllerDismissAnimationDidEnd| on the delegate.
- (void)dismissWithSelectedTabAnimation;

// If the stack view is not already being dismissed, switches to |targetModel|'s
// mode and performs the animation for switching out of the stack view while
// simultaneously opening a tab with |url| in |targetModel| at the given
// |position| with |transition|. Otherwise, simply returns nil.
// If |position| == NSNotFound the tab will be added at the end of the stack.
- (Tab*)dismissWithNewTabAnimationToModel:(TabModel*)targetModel
                                  withURL:(const GURL&)url
                                  atIndex:(NSUInteger)position
                               transition:(ui::PageTransition)transition;

@end

#endif  // IOS_CHROME_BROWSER_UI_STACK_VIEW_STACK_VIEW_CONTROLLER_H_
