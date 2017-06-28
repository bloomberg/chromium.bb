// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_utils.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsViewControllerUtils

+ (void)viewControllerWillEndDragging:
            (ContentSuggestionsViewController*)suggestionsViewController
                          withYOffset:(CGFloat)yOffset
                        pinnedYOffset:(CGFloat)pinnedYOffset
                       draggingUpward:(BOOL)draggingUpward
                  targetContentOffset:(inout CGPoint*)targetContentOffset {
  CGFloat targetY = targetContentOffset->y;
  if (yOffset > 0 && yOffset < pinnedYOffset) {
    // Omnibox is currently between middle and top of screen.
    if (draggingUpward) {  // scrolling upwards
      if (targetY < pinnedYOffset) {
        // Scroll the omnibox up to |pinnedYOffset| if velocity is upwards but
        // scrolling will stop before reaching |pinnedYOffset|.
        targetContentOffset->y = yOffset;
        [suggestionsViewController.collectionView
            setContentOffset:CGPointMake(0, pinnedYOffset)
                    animated:YES];
      }
      suggestionsViewController.scrolledToTop = YES;
    } else {  // scrolling downwards
      if (targetY > 0) {
        // Scroll the omnibox down to zero if velocity is downwards or 0 but
        // scrolling will stop before reaching 0.
        targetContentOffset->y = yOffset;
        [suggestionsViewController.collectionView setContentOffset:CGPointZero
                                                          animated:YES];
      }
      suggestionsViewController.scrolledToTop = NO;
    }
  } else if (yOffset > pinnedYOffset &&
             targetContentOffset->y < pinnedYOffset) {
    // Most visited cells are currently scrolled up past the omnibox but will
    // end the scroll below the omnibox. Stop the scroll at just below the
    // omnibox.
    targetContentOffset->y = yOffset;
    [suggestionsViewController.collectionView
        setContentOffset:CGPointMake(0, pinnedYOffset)
                animated:YES];
    suggestionsViewController.scrolledToTop = YES;
  } else if (yOffset >= pinnedYOffset) {
    suggestionsViewController.scrolledToTop = YES;
  } else if (yOffset <= 0) {
    suggestionsViewController.scrolledToTop = NO;
  }
}

@end
