// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FEATURES_H_
#define COMPONENTS_VIZ_COMMON_FEATURES_H_

#include "components/viz/common/viz_common_export.h"

#include "base/feature_list.h"

namespace features {

VIZ_COMMON_EXPORT extern const base::Feature kEnableDrawOcclusion;
VIZ_COMMON_EXPORT extern const base::Feature kEnableSurfaceSynchronization;
VIZ_COMMON_EXPORT extern const base::Feature kEnableInvariantsViolationLogging;
VIZ_COMMON_EXPORT extern const base::Feature kEnableVizHitTestDrawQuad;
VIZ_COMMON_EXPORT extern const base::Feature kEnableVizHitTestSurfaceLayer;
VIZ_COMMON_EXPORT extern const base::Feature kVizDisplayCompositor;

VIZ_COMMON_EXPORT bool IsDrawOcclusionEnabled();
VIZ_COMMON_EXPORT bool IsSurfaceSynchronizationEnabled();
VIZ_COMMON_EXPORT bool IsSurfaceInvariantsViolationLoggingEnabled();
VIZ_COMMON_EXPORT bool IsVizHitTestingDrawQuadEnabled();
VIZ_COMMON_EXPORT bool IsVizHitTestingEnabled();
VIZ_COMMON_EXPORT bool IsVizHitTestingSurfaceLayerEnabled();

}  // namespace features

#endif  // COMPONENTS_VIZ_COMMON_FEATURES_H_
