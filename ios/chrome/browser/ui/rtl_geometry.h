// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTL_GEOMETRY_H_
#define RTL_GEOMETRY_H_

#include <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#include "base/i18n/rtl.h"

// Utilities for direction-independent layout calculations and related
// functions.

// True if views should be laid out with full RTL mirroring.
bool UseRTLLayout();

// RIGHT_TO_LEFT if UseRTLLayout(), otherwise LEFT_TO_RIGHT.
base::i18n::TextDirection LayoutDirection();

// A LayoutRect contains the information needed to generate a CGRect that may or
// may not be flipped if positioned in RTL or LTR contexts. |leading| is the
// distance from the leading edge that the final rect's edge should be; in LTR
// this will be the x-origin, in RTL it will be used to compute the x-origin.
// |boundingWidth| is the width of whatever element the rect will be used to
// frame a subview of. |originY| will be origin.y of the rect, and |size| will
// be the size of the rect.
struct LayoutRect {
  CGFloat leading;
  CGFloat boundingWidth;
  CGFloat originY;
  CGSize size;
};

// The null LayoutRect, with leading, boundingWidth and originY of 0.0, and
// a size of CGSizeZero.
extern const LayoutRect LayoutRectZero;

// Returns a new LayoutRect; |height| and |width| are used to construct the
// |size| field.
LayoutRect LayoutRectMake(CGFloat leading,
                          CGFloat boundingWidth,
                          CGFloat originY,
                          CGFloat width,
                          CGFloat height);

// Given |layout|, returns the rect for that layout in text direction
// |direction|.
CGRect LayoutRectGetRectUsingDirection(LayoutRect layout,
                                       base::i18n::TextDirection direction);
// As above, using |direction| == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
CGRect LayoutRectGetRect(LayoutRect layout);

// Utilities for getting CALayer positioning values from a layoutRect.
// Given |layout|, return the bounds rectangle of the generated rect -- that is,
// a rect with origin (0,0) and size equal to |layout|'s size.
CGRect LayoutRectGetBoundsRect(LayoutRect layout);

// Given |layout| and some anchor point |anchor| (defined in the way that
// CALayer's anchorPoint property is), return the CGPoint that defines the
// position of a rect in the context used by |layout|.
CGPoint LayoutRectGetPositionForAnchorUsingDirection(
    LayoutRect layout,
    CGPoint anchor,
    base::i18n::TextDirection direction);

// As above, using |direction| == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
CGPoint LayoutRectGetPositionForAnchor(LayoutRect layout, CGPoint anchor);

// Given |rect|, a rect, and |boundingRect|, a rect whose bounds are the
// context in which |rect|'s frame is interpreted, return the layout that
// defines |rect|, assuming |direction| is the direction |rect| was positioned
// under.
LayoutRect LayoutRectForRectInBoundingRectUsingDirection(
    CGRect rect,
    CGRect boundingRect,
    base::i18n::TextDirection direction);

// As above, using |direction| == RIGHT_TO_LEFT if UseRTLLayout(), LEFT_TO_RIGHT
// otherwise.
LayoutRect LayoutRectForRectInBoundingRect(CGRect rect, CGRect boundingRect);

// Given a layout |layout|, return the layout that defines the leading area up
// to |layout|.
LayoutRect LayoutRectGetLeadingLayout(LayoutRect layout);

// Given a layout |layout|, return the layout that defines the trailing area
// after |layout|.
LayoutRect LayoutRectGetTrailingLayout(LayoutRect layout);

// Return the trailing extent of |layout| (its leading plus its width).
CGFloat LayoutRectGetTrailingEdge(LayoutRect layout);

// Utilities for mapping UIKit geometric structures to RTL-independent geometry.

// Get leading and trailing edges of |rect|, assuming layout direction
// |direction|.
CGFloat CGRectGetLeadingEdgeUsingDirection(CGRect rect,
                                           base::i18n::TextDirection direction);
CGFloat CGRectGetTrailingEdgeUsingDirection(
    CGRect rect,
    base::i18n::TextDirection direction);

// As above, with |direction| == LayoutDirection().
CGFloat CGRectGetLeadingEdge(CGRect rect);
CGFloat CGRectGetTrailingEdge(CGRect rect);

// Leading/trailing autoresizing masks. 'Leading' is 'Left' under iOS <= 8 or
// in an LTR language, 'Right' otherwise; 'Trailing' is the obverse.
UIViewAutoresizing UIViewAutoresizingFlexibleLeadingMargin();
UIViewAutoresizing UIViewAutoresizingFlexibleTrailingMargin();

// Text-direction aware UIEdgeInsets constructor; just like UIEdgeInsetsMake(),
// except |leading| and |trailing| map to left and right when |direction| is
// LEFT_TO_RIGHT, and are swapped for RIGHT_TO_LEFT.
UIEdgeInsets UIEdgeInsetsMakeUsingDirection(
    CGFloat top,
    CGFloat leading,
    CGFloat bottom,
    CGFloat trailing,
    base::i18n::TextDirection direction);
// As above, but uses LayoutDirection() for |direction|.
UIEdgeInsets UIEdgeInsetsMakeDirected(CGFloat top,
                                      CGFloat leading,
                                      CGFloat bottom,
                                      CGFloat trailing);

// Inverses of the above functions: return the leading/trailing inset for
// the current direction.
CGFloat UIEdgeInsetsGetLeading(UIEdgeInsets insets);
CGFloat UIEdgeInsetsGetTrailing(UIEdgeInsets insets);

// Autolayout utilities

// Returns the correct NSLayoutFormatOption for the current OS and build. This
// will return NSLayoutFormatDirectionLeadingToTrailing when a full RTL flip
// is correct, and NSLayoutFormatDirectionLeftToRight when layout should not
// change with text direction.
// Generally speaking this option should be applied to any whole-page layouts;
// smaller sections of views should be determined case by case.
NSLayoutFormatOptions LayoutOptionForRTLSupport();

// Deprecated -- use UseRTLLayout() instead.
// Whether the UI is configured for right to left layout.
// The implementation will use the local in order to get the UI layout direction
// for version of iOS under 9.
// TODO(jbbegue): Use base::i18n::IsRTL() instead when it will support RTL
// pseudo language. Remove that method once base::i18n::IsRTL() is fixed.
// crbug/514625.
bool IsRTLUILayout();

#endif  // RTL_GEOMETRY_H_
