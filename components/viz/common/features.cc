// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include "base/command_line.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "components/viz/common/switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace features {

// Enables running draw occlusion algorithm to remove Draw Quads that are not
// shown on screen from CompositorFrame.
const base::Feature kEnableDrawOcclusion{"DrawOcclusion",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(USE_AURA) || defined(OS_MACOSX)
const base::Feature kEnableSurfaceSynchronization{
    "SurfaceSynchronization", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kEnableSurfaceSynchronization{
    "SurfaceSynchronization", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables DumpWithoutCrashing of surface invariants violations.
const base::Feature kEnableInvariantsViolationLogging{
    "InvariantsViolationLogging", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables running the display compositor as part of the viz service in the GPU
// process. This is also referred to as out-of-process display compositor
// (OOP-D).
const base::Feature kVizDisplayCompositor{"VizDisplayCompositor",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables running the Viz-assisted hit-test logic.
const base::Feature kEnableVizHitTestDrawQuad{
    "VizHitTestDrawQuad", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableVizHitTestSurfaceLayer{
    "VizHitTestSurfaceLayer", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SkiaRenderer.
const base::Feature kUseSkiaDeferredDisplayList{
    "UseSkiaDeferredDisplayList", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the Skia deferred display list.
const base::Feature kUseSkiaRenderer{"UseSkiaRenderer",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

bool IsSurfaceSynchronizationEnabled() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return base::FeatureList::IsEnabled(kEnableSurfaceSynchronization) ||
         command_line->HasSwitch(switches::kEnableSurfaceSynchronization) ||
         IsVizDisplayCompositorEnabled();
}

bool IsSurfaceInvariantsViolationLoggingEnabled() {
  return IsSurfaceSynchronizationEnabled() &&
         base::FeatureList::IsEnabled(kEnableInvariantsViolationLogging);
}

bool IsVizHitTestingDrawQuadEnabled() {
  return base::FeatureList::IsEnabled(kEnableVizHitTestDrawQuad) ||
         IsVizDisplayCompositorEnabled();
}

bool IsVizHitTestingEnabled() {
  return IsVizHitTestingDrawQuadEnabled() ||
         IsVizHitTestingSurfaceLayerEnabled();
}

bool IsVizHitTestingSurfaceLayerEnabled() {
  // TODO(riajiang): Check feature flag as well. https://crbug.com/804888
  // TODO(riajiang): Check kVizDisplayCompositor feature when it works with
  // that config.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kUseVizHitTestSurfaceLayer) ||
         base::FeatureList::IsEnabled(kEnableVizHitTestSurfaceLayer);
}

bool IsDrawOcclusionEnabled() {
  return base::FeatureList::IsEnabled(kEnableDrawOcclusion);
}

bool IsUsingSkiaRenderer() {
  return base::FeatureList::IsEnabled(kUseSkiaRenderer);
}

bool IsUsingSkiaDeferredDisplayList() {
  return IsUsingSkiaRenderer() &&
         base::FeatureList::IsEnabled(kUseSkiaDeferredDisplayList) &&
         IsVizDisplayCompositorEnabled();
}

bool IsVizDisplayCompositorEnabled() {
// To work around current bugs, some Android configs do not allow for Viz to be
// enabled.
// TODO(cblume): Remove this once https://crbug.com/867453 is addressed.
#if defined(OS_ANDROID) && defined(ARCH_CPU_X86_FAMILY)
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_KITKAT) {
    return false;
  }
#endif  // defined(OS_ANDROID) && defined(ARCH_CPU_X86_FAMILY)

  return base::FeatureList::IsEnabled(kVizDisplayCompositor);
}

}  // namespace features
