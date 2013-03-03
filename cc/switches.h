// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "cc" command-line switches.

#ifndef CC_SWITCHES_H_
#define CC_SWITCHES_H_

#include "cc/cc_export.h"

// Since cc is used from the render process, anything that goes here also needs
// to be added to render_process_host_impl.cc.

namespace cc {
namespace switches {

CC_EXPORT extern const char kBackgroundColorInsteadOfCheckerboard[];
CC_EXPORT extern const char kDisableThreadedAnimation[];
CC_EXPORT extern const char kEnableCompositorFrameMessage[];
CC_EXPORT extern const char kDisableImplSidePainting[];
CC_EXPORT extern const char kEnableImplSidePainting[];
CC_EXPORT extern const char kEnablePartialSwap[];
CC_EXPORT extern const char kEnablePerTilePainting[];
CC_EXPORT extern const char kEnableRightAlignedScheduling[];
CC_EXPORT extern const char kEnableTopControlsPositionCalculation[];
CC_EXPORT extern const char kJankInsteadOfCheckerboard[];
CC_EXPORT extern const char kNumRasterThreads[];
CC_EXPORT extern const char kShowPropertyChangedRects[];
CC_EXPORT extern const char kShowSurfaceDamageRects[];
CC_EXPORT extern const char kShowScreenSpaceRects[];
CC_EXPORT extern const char kShowReplicaScreenSpaceRects[];
CC_EXPORT extern const char kShowOccludingRects[];
CC_EXPORT extern const char kShowNonOccludingRects[];
CC_EXPORT extern const char kTraceOverdraw[];
CC_EXPORT extern const char kTopControlsHeight[];
CC_EXPORT extern const char kTopControlsHideThreshold[];
CC_EXPORT extern const char kTopControlsShowThreshold[];
CC_EXPORT extern const char kTraceAllRenderedFrames[];
CC_EXPORT extern const char kSlowDownRasterScaleFactor[];
CC_EXPORT extern const char kUseCheapnessEstimator[];
CC_EXPORT extern const char kLowResolutionContentsScaleFactor[];
CC_EXPORT extern const char kCompositeToMailbox[];

CC_EXPORT bool IsImplSidePaintingEnabled();

}  // namespace switches
}  // namespace cc

#endif  // CC_SWITCHES_H_
