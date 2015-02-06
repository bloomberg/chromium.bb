// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_
#define CC_OUTPUT_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/overlay_candidate.h"
#include "cc/output/overlay_processor.h"
#include "cc/quads/render_pass.h"

namespace cc {
class OverlayCandidateValidator;
class StreamVideoDrawQuad;
class TextureDrawQuad;

class CC_EXPORT OverlayStrategySingleOnTop : public OverlayProcessor::Strategy {
 public:
  OverlayStrategySingleOnTop(OverlayCandidateValidator* capability_checker,
                             ResourceProvider* resource_provider);
  bool Attempt(RenderPassList* render_passes_in_draw_order,
               OverlayCandidateList* candidate_list) override;

 private:
  bool IsOverlayQuad(const DrawQuad* draw_quad);
  bool GetCandidateQuadInfo(const DrawQuad& draw_quad,
                            OverlayCandidate* quad_info);

  // Returns true if |draw_quad| will not block quads underneath from becoming
  // an overlay.
  bool IsInvisibleQuad(const DrawQuad* draw_quad);

  bool GetTextureQuadInfo(const TextureDrawQuad& quad,
                          OverlayCandidate* quad_info);
  bool GetVideoQuadInfo(const StreamVideoDrawQuad& quad,
                        OverlayCandidate* quad_info);

  OverlayCandidateValidator* capability_checker_;
  ResourceProvider* resource_provider_;
  DISALLOW_COPY_AND_ASSIGN(OverlayStrategySingleOnTop);
};

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_STRATEGY_SINGLE_ON_TOP_H_
