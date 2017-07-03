// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_delegate.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kShiftTilesDownAnimationDuration = 0.2;
const CGFloat kShiftTilesUpAnimationDuration = 0.25;
}  // namespace

@interface ContentSuggestionsHeaderSynchronizer ()<UIGestureRecognizerDelegate>

@property(nonatomic, weak, readonly) UICollectionView* collectionView;
// |YES| if the fakebox header should be animated on scroll.
@property(nonatomic, assign) BOOL shouldAnimateHeader;
@property(nonatomic, weak) id<ContentSuggestionsCollectionControlling>
    collectionController;
@property(nonatomic, weak) id<ContentSuggestionsHeaderControlling>
    headerController;
@property(nonatomic, assign) CFTimeInterval shiftTilesDownStartTime;

// Tap and swipe gesture recognizers when the omnibox is focused.
@property(nonatomic, strong) UITapGestureRecognizer* tapGestureRecognizer;
@property(nonatomic, strong) UISwipeGestureRecognizer* swipeGestureRecognizer;

@end

@implementation ContentSuggestionsHeaderSynchronizer

@synthesize collectionController = _collectionController;
@synthesize headerController = _headerController;
@synthesize shouldAnimateHeader = _shouldAnimateHeader;
@synthesize shiftTilesDownStartTime = _shiftTilesDownStartTime;
@synthesize tapGestureRecognizer = _tapGestureRecognizer;
@synthesize swipeGestureRecognizer = _swipeGestureRecognizer;

- (instancetype)
initWithCollectionController:
    (id<ContentSuggestionsCollectionControlling>)collectionController
            headerController:
                (id<ContentSuggestionsHeaderControlling>)headerController {
  self = [super init];
  if (self) {
    _shiftTilesDownStartTime = -1;
    _shouldAnimateHeader = YES;

    _tapGestureRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(unfocusOmnibox)];
    [_tapGestureRecognizer setDelegate:self];
    _swipeGestureRecognizer = [[UISwipeGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(unfocusOmnibox)];
    [_swipeGestureRecognizer
        setDirection:UISwipeGestureRecognizerDirectionDown];

    _headerController = headerController;
    _collectionController = collectionController;
  }
  return self;
}

#pragma mark - ContentSuggestionsCollectionSynchronizing

- (void)shiftTilesDown {
  self.shouldAnimateHeader = YES;
  self.collectionController.scrolledToTop = NO;

  [self.collectionView removeGestureRecognizer:self.tapGestureRecognizer];
  [self.collectionView removeGestureRecognizer:self.swipeGestureRecognizer];

  // CADisplayLink is used for this animation instead of the standard UIView
  // animation because the standard animation did not properly convert the
  // fakebox from its scrolled up mode to its scrolled down mode. Specifically,
  // calling |UICollectionView reloadData| adjacent to the standard animation
  // caused the fakebox's views to jump incorrectly. CADisplayLink avoids this
  // problem because it allows |shiftTilesDownAnimationDidFire| to directly
  // control each frame.
  CADisplayLink* link = [CADisplayLink
      displayLinkWithTarget:self
                   selector:@selector(shiftTilesDownAnimationDidFire:)];
  [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)shiftTilesUpWithCompletionBlock:(ProceduralBlock)completion {
  self.collectionController.scrolledToTop = YES;
  // Add gesture recognizer to collection view when the omnibox is focused.
  [self.collectionView addGestureRecognizer:self.tapGestureRecognizer];
  [self.collectionView addGestureRecognizer:self.swipeGestureRecognizer];

  CGFloat pinnedOffsetY =
      [self.collectionController.suggestionsDelegate pinnedOffsetY];
  self.shouldAnimateHeader = !IsIPadIdiom();

  [UIView animateWithDuration:kShiftTilesUpAnimationDuration
      animations:^{
        if (self.collectionView.contentOffset.y < pinnedOffsetY) {
          self.collectionView.contentOffset = CGPointMake(0, pinnedOffsetY);
          [self.collectionView.collectionViewLayout invalidateLayout];
        }
      }
      completion:^(BOOL finished) {
        // Check to see if the collection are still scrolled to the top -- it's
        // possible (and difficult) to unfocus the omnibox and initiate a
        // -shiftTilesDown before the animation here completes.
        if (self.collectionController.scrolledToTop) {
          self.shouldAnimateHeader = NO;
          if (completion)
            completion();
        }
      }];
}

#pragma mark - ContentSuggestionsHeaderSynchronizing

- (void)updateFakeOmniboxForScrollView:(UIScrollView*)scrollView {
  // Unfocus the omnibox when the scroll view is scrolled below the pinned
  // offset.
  CGFloat pinnedOffsetY =
      [self.collectionController.suggestionsDelegate pinnedOffsetY];
  if ([self.headerController isOmniboxFocused] && scrollView.dragging &&
      scrollView.contentOffset.y < pinnedOffsetY) {
    [self.headerController unfocusOmnibox];
  }

  if (IsIPadIdiom()) {
    return;
  }

  if (self.shouldAnimateHeader) {
    [self.headerController
        updateSearchFieldForOffset:self.collectionView.contentOffset.y];
  }
}

- (void)unfocusOmnibox {
  [self.headerController unfocusOmnibox];
}

#pragma mark - Private

// Convenience method to get the collection view of the suggestions.
- (UICollectionView*)collectionView {
  return [self.collectionController collectionView];
}

// Updates the collection view's scroll view offset for the next frame of the
// shiftTilesDown animation.
- (void)shiftTilesDownAnimationDidFire:(CADisplayLink*)link {
  // If this is the first frame of the animation, store the starting timestamp
  // and do nothing.
  if (self.shiftTilesDownStartTime == -1) {
    self.shiftTilesDownStartTime = link.timestamp;
    return;
  }

  CFTimeInterval timeElapsed = link.timestamp - self.shiftTilesDownStartTime;
  double percentComplete = timeElapsed / kShiftTilesDownAnimationDuration;
  // Ensure that the percentage cannot be above 1.0.
  if (percentComplete > 1.0)
    percentComplete = 1.0;

  // Find how much the collection view should be scrolled up in the next frame.
  CGFloat yOffset =
      (1.0 - percentComplete) *
      [self.collectionController.suggestionsDelegate pinnedOffsetY];
  self.collectionView.contentOffset = CGPointMake(0, yOffset);

  if (percentComplete == 1.0) {
    [link invalidate];
    // Reset |shiftTilesDownStartTime to its sentinel value.
    self.shiftTilesDownStartTime = -1;
    [self.collectionView.collectionViewLayout invalidateLayout];
  }
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  BOOL isMostVisitedCell =
      content_suggestions::nearestAncestor(
          touch.view, [ContentSuggestionsMostVisitedCell class]) != nil;
  BOOL isSuggestionCell =
      content_suggestions::nearestAncestor(
          touch.view, [ContentSuggestionsCell class]) != nil;
  return !isMostVisitedCell && !isSuggestionCell;
}

- (UIView*)nearestAncestorOfView:(UIView*)view withClass:(Class)aClass {
  if (!view) {
    return nil;
  }
  if ([view isKindOfClass:aClass]) {
    return view;
  }
  return [self nearestAncestorOfView:[view superview] withClass:aClass];
}

@end
