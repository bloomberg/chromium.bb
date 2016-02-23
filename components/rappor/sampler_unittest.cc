// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/sampler.h"

#include <utility>

#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace rappor {

const RapporParameters kTestRapporParameters = {
    1 /* Num cohorts */,
    1 /* Bloom filter size bytes */,
    4 /* Bloom filter hash count */,
    NORMAL_NOISE /* Noise level */,
    UMA_RAPPOR_GROUP /* Recording group (not used) */};

class TestSamplerFactory {
 public:
  static scoped_ptr<Sample> CreateSample() {
    return scoped_ptr<Sample>(new Sample(0, kTestRapporParameters));
  }
};

namespace internal {

// Test that exporting deletes samples.
TEST(RapporSamplerTest, TestExport) {
  Sampler sampler;

  scoped_ptr<Sample> sample1 = TestSamplerFactory::CreateSample();
  sample1->SetStringField("Foo", "Junk");
  sampler.AddSample("Metric1", std::move(sample1));

  scoped_ptr<Sample> sample2 = TestSamplerFactory::CreateSample();
  sample2->SetStringField("Foo", "Junk2");
  sampler.AddSample("Metric1", std::move(sample2));

  // Since the two samples were for one metric, we should randomly get one
  // of the two.
  RapporReports reports;
  std::string secret = HmacByteVectorGenerator::GenerateEntropyInput();
  sampler.ExportMetrics(secret, &reports);
  EXPECT_EQ(1, reports.report_size());
  EXPECT_EQ(1u, reports.report(0).bits().size());

  // First export should clear the metric.
  RapporReports reports2;
  sampler.ExportMetrics(secret, &reports2);
  EXPECT_EQ(0, reports2.report_size());
}

}  // namespace internal

}  // namespace rappor
