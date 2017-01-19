// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/contextual_search/panel_configuration.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

namespace {
// Amount of tab that a previewing pane leaves visible, expressed as a fraction.
const CGFloat kPhonePreviewingDisplayRatio = 1.0 / 3.0;
// Phone peeking height equals phone toolbar height.
const CGFloat kPhonePeekingHeight = 64.0;
// Phone covering offset equals the status bar height;
const CGFloat kPhoneCoveringHeightOffset = 20.0;

const CGFloat kPadPreviewingDisplayRatio = 1.0 / 3.0;
// iPad peeking height equals iPad toolbar height.
const CGFloat kPadPeekingHeight = 56.0;
// Pad covering offset = tab strip height (39) + status bar height (20).
const CGFloat kPadCoveringHeightOffset = 59.0;
// Offset to move the pad covering height down by so it looks better.
const CGFloat kPadCoveringHeightDecrease = 30.0;
// Width ratio for regular panel sizes.
const CGFloat kRegularPanelWidthRatio = 0.6;

// Struct wrapper for passing around a C array.
typedef struct { CGFloat byState[ContextualSearch::COVERING + 1]; } Positions;
}

@interface PanelConfiguration ()
@property(nonatomic, assign) Positions positions;
- (void)updatePositions;
@end

@implementation PanelConfiguration

@synthesize containerSize = _containerSize;
@synthesize positions = _positions;
@synthesize horizontalSizeClass = _horizontalSizeClass;

+ (instancetype)configurationForContainerSize:(CGSize)containerSize
                          horizontalSizeClass:
                              (UIUserInterfaceSizeClass)horizontalSizeClass {
  PanelConfiguration* config = [[[self alloc] init] autorelease];
  config.containerSize = containerSize;
  config.horizontalSizeClass = horizontalSizeClass;
  return config;
}

- (void)setContainerSize:(CGSize)containerSize {
  DCHECK(containerSize.height > self.peekingHeight);
  _containerSize = containerSize;
  [self updatePositions];
}

- (CGFloat)peekingHeight {
  NOTIMPLEMENTED()
      << "PanelConfiguation is an abstract superclass; use a concrete subclass";
  return 0.0;
}

- (void)updatePositions {
  NOTIMPLEMENTED()
      << "PanelConfiguation is an abstract superclass; use a concrete subclass";
}

- (CGFloat)positionForPanelState:(ContextualSearch::PanelState)state {
  return self.positions.byState[state];
}

- (ContextualSearch::PanelState)panelStateForPosition:(CGFloat)position {
  for (NSInteger i = ContextualSearch::DISMISSED;
       i < ContextualSearch::COVERING; i++) {
    if (position > self.positions.byState[i + 1])
      return static_cast<ContextualSearch::PanelState>(i);
  }
  return ContextualSearch::COVERING;
}

- (CGFloat)gradationToState:(ContextualSearch::PanelState)toState
                  fromState:(ContextualSearch::PanelState)fromState
                 atPosition:(CGFloat)position {
  if (toState == fromState)
    return 0;
  CGFloat distance =
      fabs(self.positions.byState[toState] - self.positions.byState[fromState]);
  CGFloat progress = fabs(self.positions.byState[fromState] - position);
  if (fromState > toState)
    progress = distance - progress;
  return progress / distance;
}

- (NSArray*)constraintsForSizingPanel:(UIView*)panel {
  // Panels in both width classes should be exactly visible on cover mode and
  // horizontally centered in the container view.
  DCHECK(panel.superview != nil);
  NSMutableArray* constraints = [NSMutableArray arrayWithArray:@[
    [panel.heightAnchor
        constraintEqualToAnchor:panel.superview.heightAnchor
                       constant:-self.positions
                                     .byState[ContextualSearch::COVERING]],
    // This technically positions the panel, it doesn't size it.
    [panel.centerXAnchor constraintEqualToAnchor:panel.superview.centerXAnchor],
  ]];
  if (self.horizontalSizeClass == UIUserInterfaceSizeClassCompact) {
    // In a compact layout, the panel is the width of its superview.
    [constraints
        addObject:[panel.widthAnchor
                      constraintEqualToAnchor:panel.superview.widthAnchor]];
  } else if (self.horizontalSizeClass == UIUserInterfaceSizeClassRegular) {
    // In a regular layout, the panel width is |kRegularPanelWidthRatio| of its
    // superview.
    [constraints
        addObject:[panel.widthAnchor
                      constraintEqualToAnchor:panel.superview.widthAnchor
                                   multiplier:kRegularPanelWidthRatio]];
  }
  return constraints;
}

- (NSLayoutConstraint*)
constraintForPositioningGuide:(UILayoutGuide*)guide
                      atState:(ContextualSearch::PanelState)state {
  DCHECK(guide.owningView != nil);

  NSLayoutConstraint* positioningConstraint = nil;
  switch (state) {
    case ContextualSearch::DISMISSED:
      positioningConstraint = [guide.heightAnchor
          constraintEqualToAnchor:guide.owningView.heightAnchor];
      break;
    case ContextualSearch::PEEKING:
      positioningConstraint = [guide.heightAnchor
          constraintEqualToAnchor:guide.owningView.heightAnchor
                         constant:-self.peekingHeight];
      break;
    case ContextualSearch::PREVIEWING:
      // Previewing height should be provided by a subclass.
      NOTREACHED();
      break;
    case ContextualSearch::COVERING:
      positioningConstraint = [guide.heightAnchor
          constraintEqualToConstant:self.positions
                                        .byState[ContextualSearch::COVERING]];
      break;
    default:
      positioningConstraint = nil;
      NOTREACHED();
      break;
  }
  return positioningConstraint;
}
@end

@implementation PhonePanelConfiguration

- (CGFloat)peekingHeight {
  return kPhonePeekingHeight;
}

- (void)updatePositions {
  Positions newPositions;

  newPositions.byState[ContextualSearch::DISMISSED] = self.containerSize.height;
  newPositions.byState[ContextualSearch::PEEKING] =
      self.containerSize.height - self.peekingHeight;
  newPositions.byState[ContextualSearch::PREVIEWING] =
      self.containerSize.height * kPhonePreviewingDisplayRatio;
  DCHECK(newPositions.byState[ContextualSearch::PREVIEWING] <
         newPositions.byState[ContextualSearch::PEEKING]);
  newPositions.byState[ContextualSearch::COVERING] = kPhoneCoveringHeightOffset;

  self.positions = newPositions;
}

- (NSLayoutConstraint*)
constraintForPositioningGuide:(UILayoutGuide*)guide
                      atState:(ContextualSearch::PanelState)state {
  NSLayoutConstraint* positioningConstraint;
  switch (state) {
    case ContextualSearch::PREVIEWING:
      positioningConstraint = [guide.heightAnchor
          constraintEqualToAnchor:guide.owningView.heightAnchor
                       multiplier:kPhonePreviewingDisplayRatio];
      break;
    default:
      positioningConstraint =
          [super constraintForPositioningGuide:guide atState:state];
      break;
  }
  return positioningConstraint;
}

@end

@implementation PadPanelConfiguration

- (CGFloat)peekingHeight {
  return kPadPeekingHeight;
}

- (void)updatePositions {
  Positions newPositions;

  newPositions.byState[ContextualSearch::DISMISSED] = self.containerSize.height;
  newPositions.byState[ContextualSearch::PEEKING] =
      self.containerSize.height - self.peekingHeight;
  newPositions.byState[ContextualSearch::PREVIEWING] =
      self.containerSize.height * kPadPreviewingDisplayRatio;
  DCHECK(newPositions.byState[ContextualSearch::PREVIEWING] <
         newPositions.byState[ContextualSearch::PEEKING]);
  newPositions.byState[ContextualSearch::COVERING] =
      kPadCoveringHeightOffset + kPadCoveringHeightDecrease;

  self.positions = newPositions;
}

- (NSLayoutConstraint*)
constraintForPositioningGuide:(UILayoutGuide*)guide
                      atState:(ContextualSearch::PanelState)state {
  NSLayoutConstraint* positioningConstraint;
  switch (state) {
    case ContextualSearch::PREVIEWING:
      positioningConstraint = [guide.heightAnchor
          constraintEqualToAnchor:guide.owningView.heightAnchor
                       multiplier:kPadPreviewingDisplayRatio];
      break;
    default:
      positioningConstraint =
          [super constraintForPositioningGuide:guide atState:state];
      break;
  }
  return positioningConstraint;
}

@end
