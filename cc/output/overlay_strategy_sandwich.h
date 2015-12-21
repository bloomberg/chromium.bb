// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_STRATEGY_SANDWICH_H_
#define CC_OUTPUT_OVERLAY_STRATEGY_SANDWICH_H_

#include "base/macros.h"
#include "cc/output/overlay_processor.h"

namespace cc {

class OverlayCandidateValidator;

// The sandwich strategy looks for a video quad without regard to quads above
// it. The video is "overlaid" on top of the scene, and then the non-empty
// parts of the scene are "overlaid" on top of the video. This is only valid
// for overlay contents that are fully opaque.
class CC_EXPORT OverlayStrategySandwich : public OverlayProcessor::Strategy {
 public:
  explicit OverlayStrategySandwich(
      OverlayCandidateValidator* capability_checker);
  ~OverlayStrategySandwich() override;

  bool Attempt(ResourceProvider* resource_provider,
               RenderPassList* render_passes,
               OverlayCandidateList* candidate_list) override;

 private:
  QuadList::Iterator TryOverlay(RenderPass* render_pass,
                                OverlayCandidateList* candidate_list,
                                const OverlayCandidate& candidate,
                                QuadList::Iterator candidate_iterator);

  OverlayCandidateValidator* capability_checker_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(OverlayStrategySandwich);
};

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_STRATEGY_SANDWICH_H_
