// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/statistics_delta_reader.h"

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(StatisticsDeltaReaderTest, Scope) {
  // Record a histogram before the creation of the recorder.
  UMA_HISTOGRAM_BOOLEAN("Test", true);

  StatisticsDeltaReader reader;

  // Verify that no histogram is recorded.
  scoped_ptr<HistogramSamples> samples(
      reader.GetHistogramSamplesSinceCreation("Test"));
  EXPECT_FALSE(samples);

  // Record a histogram after the creation of the recorder.
  UMA_HISTOGRAM_BOOLEAN("Test", true);

  // Verify that one histogram is recorded.
  samples = reader.GetHistogramSamplesSinceCreation("Test");
  EXPECT_TRUE(samples);
  EXPECT_EQ(1, samples->TotalCount());
}

}  // namespace base
