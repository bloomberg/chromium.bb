// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/overlay_candidate_validator_android.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/test/gfx_util.h"

namespace viz {

TEST(OverlayCandidateValidatorAndroidTest, NoClipOrNegativeOffset) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(10.f, 10.f);
  candidate.uv_rect = gfx::RectF(1.f, 1.f);
  candidate.is_clipped = false;
  candidate.clip_rect = gfx::Rect(5, 5);
  candidate.overlay_handled = false;

  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  OverlayCandidateValidatorAndroid validator;
  validator.CheckOverlaySupport(&candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);
  EXPECT_RECTF_EQ(candidates.at(0).display_rect, gfx::RectF(10.f, 10.f));
}

TEST(OverlayCandidateValidatorAndroidTest, Clipped) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(10.f, 10.f);
  candidate.uv_rect = gfx::RectF(1.f, 1.f);
  candidate.is_clipped = true;
  candidate.clip_rect = gfx::Rect(2, 2, 5, 5);
  candidate.overlay_handled = false;

  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  OverlayCandidateValidatorAndroid validator;
  validator.CheckOverlaySupport(&candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);
  EXPECT_RECTF_EQ(candidates.at(0).display_rect,
                  gfx::RectF(2.f, 2.f, 5.f, 5.f));
  EXPECT_RECTF_EQ(candidates.at(0).uv_rect, gfx::RectF(0.2f, 0.2f, 0.5f, 0.5f));
}

TEST(OverlayCandidateValidatorAndroidTest, NegativeOffset) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(-2.f, -4.f, 10.f, 10.f);
  candidate.uv_rect = gfx::RectF(0.5f, 0.5f);
  candidate.is_clipped = false;
  candidate.clip_rect = gfx::Rect(5, 5);
  candidate.overlay_handled = false;

  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  OverlayCandidateValidatorAndroid validator;
  validator.CheckOverlaySupport(&candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);
  EXPECT_RECTF_EQ(candidates.at(0).display_rect,
                  gfx::RectF(0.f, 0.f, 8.f, 6.f));
  EXPECT_RECTF_EQ(candidates.at(0).uv_rect, gfx::RectF(0.1f, 0.2f, 0.4f, 0.3f));
}

TEST(OverlayCandidateValidatorAndroidTest, ClipAndNegativeOffset) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(-5.0f, -5.0f, 10.0f, 10.0f);
  candidate.uv_rect = gfx::RectF(0.5f, 0.5f, 0.5f, 0.5f);
  candidate.is_clipped = true;
  candidate.clip_rect = gfx::Rect(5, 5);
  candidate.overlay_handled = false;

  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  OverlayCandidateValidatorAndroid validator;
  validator.CheckOverlaySupport(&candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);
  EXPECT_RECTF_EQ(candidates.at(0).display_rect,
                  gfx::RectF(0.f, 0.f, 5.f, 5.f));
  EXPECT_RECTF_EQ(candidates.at(0).uv_rect,
                  gfx::RectF(0.75f, 0.75f, 0.25f, 0.25f));
}

}  // namespace viz
