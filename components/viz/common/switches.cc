// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/switches.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"

namespace switches {
namespace {

constexpr uint32_t kDefaultActivationDeadlineInFrames = 4;

}  // namespace

// The default number of the BeginFrames to wait to activate a surface with
// dependencies.
const char kDeadlineToSynchronizeSurfaces[] =
    "deadline-to-synchronize-surfaces";

// Enables multi-client Surface synchronization. In practice, this indicates
// that LayerTreeHost expects to be given a valid viz::LocalSurfaceId provided
// by the parent compositor.
const char kEnableSurfaceSynchronization[] = "enable-surface-synchronization";

// Enables the viz hit-test logic (HitTestAggregator and HitTestQuery), with
// hit-test data coming from draw quad.
const char kUseVizHitTestDrawQuad[] = "use-viz-hit-test-draw-quad";

// Enables the viz hit-test logic (HitTestAggregator and HitTestQuery), with
// hit-test data coming from surface layer.
const char kUseVizHitTestSurfaceLayer[] = "use-viz-hit-test-surface-layer";

base::Optional<uint32_t> GetDeadlineToSynchronizeSurfaces() {
  std::string deadline_to_synchronize_surfaces_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDeadlineToSynchronizeSurfaces);
  if (deadline_to_synchronize_surfaces_string.empty())
    return kDefaultActivationDeadlineInFrames;

  uint32_t activation_deadline_in_frames;
  if (!base::StringToUint(deadline_to_synchronize_surfaces_string,
                          &activation_deadline_in_frames)) {
    return base::nullopt;
  }
  return activation_deadline_in_frames;
}

}  // namespace switches
