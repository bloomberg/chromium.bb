// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/tabs/tab_util.h"

#include <sys/sysctl.h>

namespace {

// Constants for inset and control points for tab shape.
const CGFloat kInsetMultiplier = 2.0 / 3.0;
const CGFloat kControlPoint1Multiplier = 1.0 / 3.0;
const CGFloat kControlPoint2Multiplier = 3.0 / 8.0;

}  // anonymous namespace

namespace ios_internal {
namespace tab_util {

UIBezierPath* tabBezierPathForRect(CGRect rect) {
  const CGFloat lineWidth = 1.0;
  const CGFloat halfLineWidth = lineWidth / 2.0;

  // Outset by halfLineWidth in order to draw on pixels rather than on borders
  // (which would cause blurry pixels). Offset instead of outset vertically,
  // otherwise clipping will occur.
  rect = CGRectInset(rect, -halfLineWidth, 0);
  rect.origin.y += halfLineWidth;

  CGPoint bottomLeft =
      CGPointMake(CGRectGetMinX(rect), CGRectGetMaxY(rect) - (2 * lineWidth));
  CGPoint bottomRight =
      CGPointMake(CGRectGetMaxX(rect), CGRectGetMaxY(rect) - (2 * lineWidth));
  CGPoint topRight = CGPointMake(
      CGRectGetMaxX(rect) - (kInsetMultiplier * CGRectGetHeight(rect)),
      CGRectGetMinY(rect));
  CGPoint topLeft = CGPointMake(
      CGRectGetMinX(rect) + (kInsetMultiplier * CGRectGetHeight(rect)),
      CGRectGetMinY(rect));

  CGFloat baseControlPointOutset =
      CGRectGetHeight(rect) * kControlPoint1Multiplier;
  CGFloat bottomControlPointInset =
      CGRectGetHeight(rect) * kControlPoint2Multiplier;

  // Outset many of these values by lineWidth to cause the fill to bleed outside
  // the clip area.
  UIBezierPath* path = [UIBezierPath bezierPath];
  [path moveToPoint:CGPointMake(bottomLeft.x - lineWidth,
                                bottomLeft.y + (2 * lineWidth))];
  [path addLineToPoint:CGPointMake(bottomLeft.x - lineWidth, bottomLeft.y)];
  [path addLineToPoint:bottomLeft];
  [path addCurveToPoint:topLeft
          controlPoint1:CGPointMake(bottomLeft.x + baseControlPointOutset,
                                    bottomLeft.y)
          controlPoint2:CGPointMake(topLeft.x - bottomControlPointInset,
                                    topLeft.y)];
  [path addLineToPoint:topRight];
  [path addCurveToPoint:bottomRight
          controlPoint1:CGPointMake(topRight.x + bottomControlPointInset,
                                    topRight.y)
          controlPoint2:CGPointMake(bottomRight.x - baseControlPointOutset,
                                    bottomRight.y)];
  [path addLineToPoint:CGPointMake(bottomRight.x + lineWidth, bottomRight.y)];
  [path addLineToPoint:CGPointMake(bottomRight.x + lineWidth,
                                   bottomRight.y + (2 * lineWidth))];
  return path;
}

}  // namespace tab_util
}  // namespace ios_internal
