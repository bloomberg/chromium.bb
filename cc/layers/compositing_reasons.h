// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_COMPOSITING_REASONS_H_
#define CC_LAYERS_COMPOSITING_REASONS_H_

namespace cc {

// This is a clone of CompositingReasons and WebCompositingReasons from Blink.
enum {
  kCompositingReasonUnknown                                = 0,
  kCompositingReason3DTransform                            = 1 << 0,
  kCompositingReasonVideo                                  = 1 << 1,
  kCompositingReasonCanvas                                 = 1 << 2,
  kCompositingReasonPlugin                                 = 1 << 3,
  kCompositingReasonIFrame                                 = 1 << 4,
  kCompositingReasonBackfaceVisibilityHidden               = 1 << 5,
  kCompositingReasonAnimation                              = 1 << 6,
  kCompositingReasonFilters                                = 1 << 7,
  kCompositingReasonPositionFixed                          = 1 << 8,
  kCompositingReasonPositionSticky                         = 1 << 9,
  kCompositingReasonOverflowScrollingTouch                 = 1 << 10,
  kCompositingReasonBlending                               = 1 << 11,
  kCompositingReasonAssumedOverlap                         = 1 << 12,
  kCompositingReasonOverlap                                = 1 << 13,
  kCompositingReasonNegativeZIndexChildren                 = 1 << 14,
  kCompositingReasonTransformWithCompositedDescendants     = 1 << 15,
  kCompositingReasonOpacityWithCompositedDescendants       = 1 << 16,
  kCompositingReasonMaskWithCompositedDescendants          = 1 << 17,
  kCompositingReasonReflectionWithCompositedDescendants    = 1 << 18,
  kCompositingReasonFilterWithCompositedDescendants        = 1 << 19,
  kCompositingReasonBlendingWithCompositedDescendants      = 1 << 20,
  kCompositingReasonClipsCompositingDescendants            = 1 << 21,
  kCompositingReasonPerspective                            = 1 << 22,
  kCompositingReasonPreserve3D                             = 1 << 23,
  kCompositingReasonReflectionOfCompositedParent           = 1 << 24,
  kCompositingReasonRoot                                   = 1 << 25,
  kCompositingReasonLayerForClip                           = 1 << 26,
  kCompositingReasonLayerForScrollbar                      = 1 << 27,
  kCompositingReasonLayerForScrollingContainer             = 1 << 28,
  kCompositingReasonLayerForForeground                     = 1 << 29,
  kCompositingReasonLayerForBackground                     = 1 << 30,
  kCompositingReasonLayerForMask                           = 1 << 31,
};

typedef uint32 CompositingReasons;

}  // namespace cc

#endif  // CC_LAYERS_COMPOSITING_REASONS_H_
