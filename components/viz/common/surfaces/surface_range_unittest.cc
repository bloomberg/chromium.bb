// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/surfaces/surface_id.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

// Verifying that SurfaceId::IsInRangeExclusive works properly.
TEST(SurfaceRangeTest, VerifyInRange) {
  viz::FrameSinkId FrameSink1(65564, 0);
  viz::FrameSinkId FrameSink2(65565, 0);
  const base::UnguessableToken token1 = base::UnguessableToken::Create();
  const base::UnguessableToken token2 = base::UnguessableToken::Create();

  const viz::SurfaceId start(FrameSink1, viz::LocalSurfaceId(1, 1, token1));
  const viz::SurfaceId end(FrameSink1, viz::LocalSurfaceId(2, 2, token1));

  const viz::SurfaceRange surface_range1(start, end);
  const viz::SurfaceRange surface_range2(base::nullopt, end);

  const viz::SurfaceId surface_id1(FrameSink1,
                                   viz::LocalSurfaceId(1, 2, token1));
  const viz::SurfaceId surface_id2(FrameSink2,
                                   viz::LocalSurfaceId(1, 2, token2));
  const viz::SurfaceId surface_id3(FrameSink1,
                                   viz::LocalSurfaceId(2, 2, token1));
  const viz::SurfaceId surface_id4(FrameSink1,
                                   viz::LocalSurfaceId(3, 3, token1));

  // |surface_id1| has the right embed token and is inside the range
  // (Start,end).
  EXPECT_TRUE(surface_range1.IsInRangeExclusive(surface_id1));

  // |surface_id1| has the right embed token and inside the range
  // (base::nullopt,end).
  EXPECT_TRUE(surface_range2.IsInRangeExclusive(surface_id1));

  // |surface_id2| has the wrong embed token/FrameSinkId so should be refused.
  EXPECT_FALSE(surface_range1.IsInRangeExclusive(surface_id2));
  EXPECT_FALSE(surface_range2.IsInRangeExclusive(surface_id2));

  // |surface_id3| is not included in the range exclusively.
  EXPECT_FALSE(surface_range1.IsInRangeExclusive(surface_id3));
  EXPECT_FALSE(surface_range2.IsInRangeExclusive(surface_id3));

  // |surface_id4| is not included in the range.
  EXPECT_FALSE(surface_range1.IsInRangeExclusive(surface_id4));
  EXPECT_FALSE(surface_range2.IsInRangeExclusive(surface_id4));
}
