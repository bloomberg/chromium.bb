// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_COMPOSITING_REASONS_H_
#define CC_LAYERS_COMPOSITING_REASONS_H_

#include "base/port.h"

namespace cc {

// This is a clone of CompositingReasons and WebCompositingReasons from Blink.
const uint64 kCompositingReasonUnknown = 0;
const uint64 kCompositingReason3DTransform = GG_UINT64_C(1) << 0;
const uint64 kCompositingReasonVideo = GG_UINT64_C(1) << 1;
const uint64 kCompositingReasonCanvas = GG_UINT64_C(1) << 2;
const uint64 kCompositingReasonPlugin = GG_UINT64_C(1) << 3;
const uint64 kCompositingReasonIFrame = GG_UINT64_C(1) << 4;
const uint64 kCompositingReasonBackfaceVisibilityHidden = GG_UINT64_C(1) << 5;
const uint64 kCompositingReasonAnimation = GG_UINT64_C(1) << 6;
const uint64 kCompositingReasonFilters = GG_UINT64_C(1) << 7;
const uint64 kCompositingReasonPositionFixed = GG_UINT64_C(1) << 8;
const uint64 kCompositingReasonPositionSticky = GG_UINT64_C(1) << 9;
const uint64 kCompositingReasonOverflowScrollingTouch = GG_UINT64_C(1) << 10;
const uint64 kCompositingReasonAssumedOverlap = GG_UINT64_C(1) << 12;
const uint64 kCompositingReasonOverlap = GG_UINT64_C(1) << 13;
const uint64 kCompositingReasonNegativeZIndexChildren = GG_UINT64_C(1) << 14;
const uint64 kCompositingReasonTransformWithCompositedDescendants =
    GG_UINT64_C(1) << 15;
const uint64 kCompositingReasonOpacityWithCompositedDescendants =
    GG_UINT64_C(1) << 16;
const uint64 kCompositingReasonMaskWithCompositedDescendants =
    GG_UINT64_C(1) << 17;
const uint64 kCompositingReasonReflectionWithCompositedDescendants =
    GG_UINT64_C(1) << 18;
const uint64 kCompositingReasonFilterWithCompositedDescendants =
    GG_UINT64_C(1) << 19;
const uint64 kCompositingReasonBlendingWithCompositedDescendants =
    GG_UINT64_C(1) << 20;
const uint64 kCompositingReasonClipsCompositingDescendants =
    GG_UINT64_C(1) << 21;
const uint64 kCompositingReasonPerspective = GG_UINT64_C(1) << 22;
const uint64 kCompositingReasonPreserve3D = GG_UINT64_C(1) << 23;
const uint64 kCompositingReasonReflectionOfCompositedParent =
    GG_UINT64_C(1) << 24;
const uint64 kCompositingReasonRoot = GG_UINT64_C(1) << 25;
const uint64 kCompositingReasonLayerForClip = GG_UINT64_C(1) << 26;
const uint64 kCompositingReasonLayerForScrollbar = GG_UINT64_C(1) << 27;
const uint64 kCompositingReasonLayerForScrollingContainer =
    GG_UINT64_C(1) << 28;
const uint64 kCompositingReasonLayerForForeground = GG_UINT64_C(1) << 29;
const uint64 kCompositingReasonLayerForBackground = GG_UINT64_C(1) << 30;
const uint64 kCompositingReasonLayerForMask = GG_UINT64_C(1) << 31;
const uint64 kCompositingReasonOverflowScrollingParent = GG_UINT64_C(1) << 32;
const uint64 kCompositingReasonOutOfFlowClipping = GG_UINT64_C(1) << 33;
const uint64 kCompositingReasonIsolateCompositedDescendants =
    GG_UINT64_C(1) << 35;

typedef uint64 CompositingReasons;

}  // namespace cc

#endif  // CC_LAYERS_COMPOSITING_REASONS_H_
