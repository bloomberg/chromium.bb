// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_STRATEGY_UNDERLAY_CAST_H_
#define CC_OUTPUT_OVERLAY_STRATEGY_UNDERLAY_CAST_H_

#include "base/macros.h"
#include "cc/output/overlay_strategy_underlay.h"

namespace cc {

class OverlayCandidateValidator;

// Similar to underlay strategy plus Cast-specific handling of content bounds.
class CC_EXPORT OverlayStrategyUnderlayCast : public OverlayStrategyUnderlay {
 public:
  explicit OverlayStrategyUnderlayCast(
      OverlayCandidateValidator* capability_checker);
  ~OverlayStrategyUnderlayCast() override;

  bool Attempt(ResourceProvider* resource_provider,
               RenderPass* render_pass,
               OverlayCandidateList* candidate_list,
               std::vector<gfx::Rect>* content_bounds) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayStrategyUnderlayCast);
};

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_STRATEGY_UNDERLAY_CAST_H_
