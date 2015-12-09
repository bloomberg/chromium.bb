// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe_gesture_recognizer.h"

#include <cmath>

#include "base/logging.h"

namespace {

// The absolute maximum swipe angle from |x = y| for a swipe to begin.
const CGFloat kMaxSwipeYAngle = 65;
// The distance between touches for a swipe to begin.
const CGFloat kMinSwipeXThreshold = 4;

}  // namespace

@implementation SideSwipeGestureRecognizer {
  // Expected direction of the swipe, based on starting point.
  UISwipeGestureRecognizerDirection _direction;
}

@synthesize swipeEdge = _swipeEdge;
@synthesize direction = _direction;
@synthesize swipeOffset = _swipeOffset;
@synthesize startPoint = _startPoint;

// To quickly avoid interference with other gesture recognizers, fail
// immediately if the touches aren't at the edge of the touched view.
- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesBegan:touches withEvent:event];

  // Don't interrupt gestures in progress.
  if (self.state != UIGestureRecognizerStatePossible)
    return;

  UITouch* touch = [[event allTouches] anyObject];
  CGPoint location = [touch locationInView:self.view];
  if (_swipeEdge > 0) {
    if (location.x > _swipeEdge &&
        location.x < CGRectGetMaxX([self.view bounds]) - _swipeEdge) {
      self.state = UIGestureRecognizerStateFailed;
    } else {
      if (location.x < _swipeEdge) {
        _direction = UISwipeGestureRecognizerDirectionRight;
      } else {
        _direction = UISwipeGestureRecognizerDirectionLeft;
      }
      _startPoint = location;
    }
  } else {
    _startPoint = location;
    _direction = 0;
  }
}

- (void)touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event {
  // Revert to normal pan gesture recognizer characteristics after state began.
  if (self.state != UIGestureRecognizerStatePossible) {
    [super touchesMoved:touches withEvent:event];
    return;
  }

  // Only one touch.
  if ([[event allTouches] count] > 1) {
    self.state = UIGestureRecognizerStateFailed;
    return;
  }

  // Don't swipe at an angle greater than |kMaxSwipeYAngle|.
  UITouch* touch = [[event allTouches] anyObject];
  CGPoint currentPoint = [touch locationInView:self.view];
  CGFloat dy = currentPoint.y - _startPoint.y;
  CGFloat dx = std::abs(currentPoint.x - _startPoint.x);
  CGFloat degrees = std::fabs(std::atan2(dy, dx) * 180 / CGFloat(M_PI));
  if (degrees > kMaxSwipeYAngle) {
    self.state = UIGestureRecognizerStateFailed;
    return;
  }

  // On devices that support force presses a -touchesMoved fires when |force|
  // changes and not the location of the touch. Ignore these events.
  if (currentPoint.x == _startPoint.x) {
    self.state = UIGestureRecognizerStatePossible;
    return;
  }

  // Don't recognize swipe in the wrong direction.
  if ((_direction == UISwipeGestureRecognizerDirectionRight &&
       currentPoint.x - _startPoint.x < 0) ||
      (_direction == UISwipeGestureRecognizerDirectionLeft &&
       currentPoint.x - _startPoint.x > 0)) {
    self.state = UIGestureRecognizerStateFailed;
    return;
  }

  if (_swipeEdge == 0 && _direction == 0) {
    if (currentPoint.x > _startPoint.x) {
      _direction = UISwipeGestureRecognizerDirectionRight;
    } else {
      _direction = UISwipeGestureRecognizerDirectionLeft;
    }
  }

  // Begin recognizer after |kMinSwipeXThreshold| distance swiped.
  if (std::abs(currentPoint.x - _startPoint.x) > kMinSwipeXThreshold) {
    if (_direction == UISwipeGestureRecognizerDirectionRight) {
      _swipeOffset = currentPoint.x;
    } else {
      _swipeOffset = -(CGRectGetMaxX([self.view bounds]) - currentPoint.x);
    }

    self.state = UIGestureRecognizerStateBegan;
    return;
  }
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  _startPoint = CGPointZero;
  [super touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  _startPoint = CGPointZero;
  [super touchesCancelled:touches withEvent:event];
}

@end
