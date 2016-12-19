// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_view.h"

#import "base/ios/crb_protocol_observers.h"
#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_protocols.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"

namespace {

// Animation timings.
const NSTimeInterval kPanelAnimationDuration = ios::material::kDuration3;
const NSTimeInterval kDismissAnimationDuration = ios::material::kDuration1;

// Elevation (in MD vertical space) of the panel when dismissed and peeking.
const CGFloat kShadowElevation = MDCShadowElevationMenu;

}  // namespace

@interface ContextualSearchPanelObservers
    : CRBProtocolObservers<ContextualSearchPanelMotionObserver>
@end
@implementation ContextualSearchPanelObservers

@end

@interface ContextualSearchPanelView ()<UIGestureRecognizerDelegate,
                                        ContextualSearchPanelMotionObserver>

// A subview whose content scrolls and whose scrolling is synchronized with
// panel dragging. This means that if the scrolling subview is being scrolled,
// that motion will not cause the panel to move, but if the scrolling reaches
// the end of its possible range, the gesture will then start dragging the
// panel.
@property(nonatomic, assign)
    UIView<ContextualSearchPanelScrollSynchronizer>* scrollSynchronizer;

// Private readonly property to be used by weak pointers to |self| for non-
// retaining access to the underlying ivar in blocks.
@property(nonatomic, readonly) ContextualSearchPanelObservers* observers;

// Utility to generate a PanelMotion struct for the panel's current position.
- (ContextualSearch::PanelMotion)motion;
@end

@implementation ContextualSearchPanelView {
  UIStackView* _contents;

  // Constraints that define the size of this view. These will be cleared and
  // regenerated when the horizontal size class changes.
  base::scoped_nsobject<NSArray> _sizingConstraints;

  CGPoint _draggingStartPosition;
  CGPoint _scrolledOffset;
  base::scoped_nsobject<UIPanGestureRecognizer> _dragRecognizer;

  base::scoped_nsobject<ContextualSearchPanelObservers> _observers;

  base::scoped_nsobject<PanelConfiguration> _configuration;

  base::WeakNSProtocol<id<ContextualSearchPanelScrollSynchronizer>>
      _scrollSynchronizer;

  // Guide that's used to position this view.
  base::WeakNSObject<UILayoutGuide> _positioningGuide;
  // Constraint that sets the size of |_positioningView| so this view is
  // positioned correctly for its state.
  base::WeakNSObject<NSLayoutConstraint> _positioningViewConstraint;
  // Other constraints that determine the position of this view.
  base::scoped_nsobject<NSArray> _positioningConstraints;

  // Promotion state variables.
  BOOL _resizingForPromotion;
  CGFloat _promotionVerticalOffset;

  // YES if dragging started inside the content view and scrolling is possible.
  BOOL _maybeScrollContent;
  // YES if the drag is happening along with scrolling the content view.
  BOOL _isScrollingContent;

  // YES if dragging upwards has occurred.
  BOOL _hasDraggedUp;
}

@synthesize state = _state;

+ (BOOL)requiresConstraintBasedLayout {
  return YES;
}

#pragma mark - Initializers

- (instancetype)initWithConfiguration:(PanelConfiguration*)configuration {
  if ((self = [super initWithFrame:CGRectZero])) {
    _configuration.reset([configuration retain]);
    _state = ContextualSearch::DISMISSED;

    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.backgroundColor = [UIColor whiteColor];
    self.accessibilityIdentifier = @"contextualSearchPanel";

    _observers.reset([[ContextualSearchPanelObservers
        observersWithProtocol:@protocol(ContextualSearchPanelMotionObserver)]
        retain]);
    [self addMotionObserver:self];

    // Add gesture recognizer.
    _dragRecognizer.reset([[UIPanGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleDragFrom:)]);
    [self addGestureRecognizer:_dragRecognizer];
    [_dragRecognizer setDelegate:self];

    // Set up the stack view that holds the panel content
    _contents = [[[UIStackView alloc] initWithFrame:self.bounds] autorelease];
    [self addSubview:_contents];
    _contents.translatesAutoresizingMaskIntoConstraints = NO;
    _contents.accessibilityIdentifier = @"panelContents";
    [NSLayoutConstraint activateConstraints:@[
      [_contents.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
      [_contents.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [_contents.widthAnchor constraintEqualToAnchor:self.widthAnchor],
      [_contents.heightAnchor constraintEqualToAnchor:self.heightAnchor]
    ]];
    _contents.axis = UILayoutConstraintAxisVertical;
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE {
  NOTREACHED();
  return nil;
}

#pragma mark - Public content views

- (void)addContentViews:(NSArray*)contentViews {
  for (UIView* view in contentViews) {
    if ([view
            conformsToProtocol:@protocol(
                                   ContextualSearchPanelScrollSynchronizer)]) {
      self.scrollSynchronizer =
          static_cast<UIView<ContextualSearchPanelScrollSynchronizer>*>(view);
    }
    if ([view conformsToProtocol:@protocol(
                                     ContextualSearchPanelMotionObserver)]) {
      [self
          addMotionObserver:static_cast<
                                id<ContextualSearchPanelMotionObserver>>(view)];
    }
    [_contents addArrangedSubview:view];
  }
}

#pragma mark - Public observer methods

- (void)addMotionObserver:(id<ContextualSearchPanelMotionObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeMotionObserver:(id<ContextualSearchPanelMotionObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)prepareForPromotion {
  self.scrollSynchronizer = nil;
  [_observers panelWillPromote:self];
}

- (void)promoteToMatchSuperviewWithVerticalOffset:(CGFloat)offset {
  _resizingForPromotion = YES;
  _promotionVerticalOffset = offset;
  [NSLayoutConstraint deactivateConstraints:_sizingConstraints];
  [NSLayoutConstraint deactivateConstraints:_positioningConstraints];
  [[_positioningGuide owningView] removeLayoutGuide:_positioningGuide];
  [_observers panelIsPromoting:self];
  [self setNeedsUpdateConstraints];
  [self updateConstraintsIfNeeded];
  [self layoutIfNeeded];
}

#pragma mark - Public property getters/setters

- (PanelConfiguration*)configuration {
  return _configuration.get();
}

- (void)setScrollSynchronizer:
    (id<ContextualSearchPanelScrollSynchronizer>)scrollSynchronizer {
  _scrollSynchronizer.reset(scrollSynchronizer);
}

- (id<ContextualSearchPanelScrollSynchronizer>)scrollSynchronizer {
  return _scrollSynchronizer;
}

- (ContextualSearchPanelObservers*)observers {
  return _observers;
}

- (void)setState:(ContextualSearch::PanelState)state {
  if (state == _state)
    return;

  [_positioningViewConstraint setActive:NO];
  _positioningViewConstraint.reset();
  base::WeakNSObject<ContextualSearchPanelView> weakSelf(self);
  void (^transform)(void) = ^{
    base::scoped_nsobject<ContextualSearchPanelView> strongSelf(
        [weakSelf retain]);
    if (strongSelf) {
      [strongSelf setNeedsUpdateConstraints];
      [[strongSelf superview] layoutIfNeeded];
      [[strongSelf observers] panel:strongSelf
                  didMoveWithMotion:[strongSelf motion]];
    }
  };

  base::mac::ScopedBlock<ProceduralBlockWithBool> completion;
  NSTimeInterval animationDuration;
  if (state == ContextualSearch::DISMISSED) {
    animationDuration = kDismissAnimationDuration;
    completion.reset(
        ^(BOOL) {
          [weakSelf setHidden:YES];
        },
        base::scoped_policy::RETAIN);
  } else {
    self.hidden = NO;
    animationDuration = kPanelAnimationDuration;
  }

  // Animations from a dismissed state are EaseOut, others are EaseInOut.
  ios::material::Curve curve = _state == ContextualSearch::DISMISSED
                                   ? ios::material::CurveEaseOut
                                   : ios::material::CurveEaseInOut;

  ContextualSearch::PanelState previousState = _state;
  _state = state;
  [_observers panel:self didChangeToState:_state fromState:previousState];

  [UIView cr_animateWithDuration:animationDuration
                           delay:0
                           curve:curve
                         options:UIViewAnimationOptionBeginFromCurrentState
                      animations:transform
                      completion:completion];
}

#pragma mark - UIView methods

- (void)updateConstraints {
  if (_resizingForPromotion) {
    [self.widthAnchor constraintEqualToAnchor:self.superview.widthAnchor]
        .active = YES;
    [self.centerXAnchor constraintEqualToAnchor:self.superview.centerXAnchor]
        .active = YES;
    [self.heightAnchor constraintEqualToAnchor:self.superview.heightAnchor
                                      constant:-_promotionVerticalOffset]
        .active = YES;
    [self.topAnchor constraintEqualToAnchor:self.superview.topAnchor
                                   constant:_promotionVerticalOffset]
        .active = YES;
  } else {
    // Don't update sizing constraints if there isn't a defined horizontal size
    // yet.
    if (self.traitCollection.horizontalSizeClass !=
            UIUserInterfaceSizeClassUnspecified &&
        !_sizingConstraints) {
      _sizingConstraints.reset(
          [[_configuration constraintsForSizingPanel:self] retain]);
      [NSLayoutConstraint activateConstraints:_sizingConstraints];
    }
    // Update positioning constraints if they don't exist.
    if (!_positioningConstraints) {
      NSArray* positioningConstraints = @[
        [[_positioningGuide topAnchor]
            constraintEqualToAnchor:self.superview.topAnchor],
        [[_positioningGuide bottomAnchor]
            constraintEqualToAnchor:self.topAnchor]
      ];
      [NSLayoutConstraint activateConstraints:positioningConstraints];

      _positioningConstraints.reset([positioningConstraints retain]);
    }
    // Always update the positioning view constraint.
    _positioningViewConstraint.reset([self.configuration
        constraintForPositioningGuide:_positioningGuide
                              atState:self.state]);
    [_positioningViewConstraint setActive:YES];
  }
  [super updateConstraints];
}

- (void)didMoveToSuperview {
  if (!self.superview)
    return;
  // Set up the invisible positioning view used to constrain this view's
  // position.
  UILayoutGuide* positioningGuide = [[[UILayoutGuide alloc] init] autorelease];
  positioningGuide.identifier = @"contextualSearchPosition";
  [self.superview addLayoutGuide:positioningGuide];
  _positioningGuide.reset(positioningGuide);
  [self setNeedsUpdateConstraints];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  if (previousTraitCollection.horizontalSizeClass ==
      self.traitCollection.horizontalSizeClass) {
    return;
  }

  [self dismissPanel];

  [_configuration
      setHorizontalSizeClass:self.traitCollection.horizontalSizeClass];
  [NSLayoutConstraint deactivateConstraints:_sizingConstraints];
  _sizingConstraints.reset();
  [self setNeedsUpdateConstraints];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.configuration.containerSize = self.superview.bounds.size;
  // Update the shadow path for this view.
  // Consider switching to "full" MDCShadowLayer.
  MDCShadowMetrics* metrics =
      [MDCShadowMetrics metricsWithElevation:kShadowElevation];
  UIBezierPath* shadowPath = [UIBezierPath bezierPathWithRect:self.bounds];
  self.layer.shadowPath = shadowPath.CGPath;
  self.layer.shadowOpacity = metrics.topShadowOpacity;
  self.layer.shadowRadius = metrics.topShadowRadius;
}

- (void)dismissPanel {
  ContextualSearch::PanelMotion motion;
  motion.state = ContextualSearch::DISMISSED;
  motion.nextState = ContextualSearch::DISMISSED;
  motion.gradation = 0;
  motion.position = 0;
  [_observers panel:self didStopMovingWithMotion:motion];
}

- (void)dealloc {
  [self removeMotionObserver:self];
  [self removeGestureRecognizer:_dragRecognizer];
  [[_positioningGuide owningView] removeLayoutGuide:_positioningGuide];
  [super dealloc];
}

#pragma mark - Gesture recognizer callbacks

- (void)handleDragFrom:(UIGestureRecognizer*)gestureRecognizer {
  UIPanGestureRecognizer* recognizer =
      static_cast<UIPanGestureRecognizer*>(gestureRecognizer);
  if ([recognizer state] == UIGestureRecognizerStateCancelled) {
    recognizer.enabled = YES;
    [self dismissPanel];
    return;
  }

  CGPoint dragOffset = [recognizer translationInView:[self superview]];
  BOOL isScrolling = NO;
  if (_maybeScrollContent && self.scrollSynchronizer.scrolled) {
    isScrolling = YES;
    _scrolledOffset = dragOffset;
  } else {
    // Adjust drag offset for prior scrolling
    dragOffset.y -= _scrolledOffset.y;
  }

  CGPoint newOrigin = _draggingStartPosition;
  newOrigin.y += dragOffset.y;

  // Clamp the drag to covering height.
  CGFloat coveringY =
      [self.configuration positionForPanelState:ContextualSearch::COVERING];
  if (newOrigin.y < coveringY) {
    newOrigin.y = coveringY;
    dragOffset.y = coveringY - _draggingStartPosition.y;
    [recognizer setTranslation:dragOffset inView:[self superview]];
  }

  // If the view hasn't moved up yet and it's moving down (dragOffset.y > 0)
  // and it's moving from a peeking state, clamp the offset y to 0.
  if (_state == ContextualSearch::PEEKING && !_hasDraggedUp &&
      dragOffset.y > 0) {
    dragOffset.y = 0;
    [recognizer setTranslation:dragOffset inView:[self superview]];
  }

  switch ([recognizer state]) {
    case UIGestureRecognizerStateBegan:
      _draggingStartPosition = self.frame.origin;
      _scrolledOffset = CGPointZero;
      _hasDraggedUp = NO;
      _maybeScrollContent = CGRectContainsPoint(
          self.scrollSynchronizer.frame, [recognizer locationInView:self]);
      break;
    case UIGestureRecognizerStateEnded:
      if (!CGPointEqualToPoint(self.frame.origin, _draggingStartPosition))
        [_observers panel:self didStopMovingWithMotion:[self motion]];
      break;
    case UIGestureRecognizerStateChanged: {
      if (!_hasDraggedUp && dragOffset.y < 0)
        _hasDraggedUp = YES;

      // Don't drag the pane if scrolling is happening.
      if (isScrolling)
        break;

      CGRect frame = self.frame;
      frame.origin.y = _draggingStartPosition.y + dragOffset.y;
      self.frame = frame;
      [_observers panel:self didMoveWithMotion:[self motion]];
    } break;
    default:
      break;
  }
}

- (ContextualSearch::PanelMotion)motion {
  ContextualSearch::PanelMotion motion;
  motion.position = self.frame.origin.y;
  motion.state = [self.configuration panelStateForPosition:motion.position];
  motion.nextState = static_cast<ContextualSearch::PanelState>(
      MIN(motion.state + 1, ContextualSearch::COVERING));
  motion.gradation = [_configuration gradationToState:motion.nextState
                                            fromState:motion.state
                                           atPosition:motion.position];
  return motion;
}

#pragma mark - ContextualSearchPanelMotionDelegate methods

- (void)panel:(ContextualSearchPanelView*)panel
    didMoveWithMotion:(ContextualSearch::PanelMotion)motion {
  if (motion.state == ContextualSearch::PREVIEWING) {
    MDCShadowMetrics* metrics =
        [MDCShadowMetrics metricsWithElevation:kShadowElevation];
    self.layer.shadowOpacity =
        metrics.topShadowOpacity * (1.0 - motion.gradation);
  }
}

#pragma mark - UIGestureRecognizerDelegate methods

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Allow the drag recognizer and the panel content scroll recognizer to
  // co-recognize.
  if (gestureRecognizer == _dragRecognizer.get() &&
      otherGestureRecognizer == self.scrollSynchronizer.scrollRecognizer) {
    return YES;
  }

  if (gestureRecognizer == _dragRecognizer.get() &&
      [_dragRecognizer state] == UIGestureRecognizerStateChanged) {
    [gestureRecognizer setEnabled:NO];
  }
  return NO;
}

@end
