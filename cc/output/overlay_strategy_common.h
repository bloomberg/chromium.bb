// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_STRATEGY_COMMON_H_
#define CC_OUTPUT_OVERLAY_STRATEGY_COMMON_H_

#include "cc/base/cc_export.h"
#include "cc/output/overlay_candidate.h"
#include "cc/output/overlay_processor.h"

namespace cc {
class OverlayCandidateValidator;
class StreamVideoDrawQuad;
class TextureDrawQuad;
class OverlayCandidate;

class CC_EXPORT OverlayStrategyCommon : public OverlayProcessor::Strategy {
 public:
  explicit OverlayStrategyCommon(OverlayCandidateValidator* capability_checker);
  ~OverlayStrategyCommon() override;

  bool Attempt(RenderPassList* render_passes_in_draw_order,
               OverlayCandidateList* candidate_list) override;

 protected:
  bool GetCandidateQuadInfo(const DrawQuad& draw_quad,
                            OverlayCandidate* quad_info);

  // Returns true if |draw_quad| will not block quads underneath from becoming
  // an overlay.
  bool IsInvisibleQuad(const DrawQuad* draw_quad);

  // Returns true if |draw_quad| is of a known quad type and contains an
  // overlayable resource.
  bool IsOverlayQuad(const DrawQuad* draw_quad);

  bool GetTextureQuadInfo(const TextureDrawQuad& quad,
                          OverlayCandidate* quad_info);
  bool GetVideoQuadInfo(const StreamVideoDrawQuad& quad,
                        OverlayCandidate* quad_info);

  virtual bool TryOverlay(OverlayCandidateValidator* capability_checker,
                          RenderPassList* render_passes_in_draw_order,
                          OverlayCandidateList* candidate_list,
                          const OverlayCandidate& candidate,
                          QuadList::Iterator iter) = 0;

 private:
  OverlayCandidateValidator* capability_checker_;

  DISALLOW_COPY_AND_ASSIGN(OverlayStrategyCommon);
};
}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_STRATEGY_COMMON_H_
