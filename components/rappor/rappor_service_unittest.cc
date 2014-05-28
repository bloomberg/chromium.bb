// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/rappor_service.h"

#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/rappor_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace rappor {

class TestRapporService : public RapporService {
 public:
  void GetReports(RapporReports* reports) {
    ExportMetrics(reports);
  }
  void TestRecordSample(const std::string& metric_name,
                        const RapporParameters& parameters,
                        const std::string& sample) {
    RecordSampleInternal(metric_name, parameters, sample);
  }
};

TEST(RapporServiceTest, RecordAndExportMetrics) {
  const RapporParameters kTestRapporParameters = {
      1 /* Num cohorts */,
      16 /* Bloom filter size bytes */,
      4 /* Bloom filter hash count */,
      PROBABILITY_75 /* Fake data probability */,
      PROBABILITY_50 /* Fake one probability */,
      PROBABILITY_75 /* One coin probability */,
      PROBABILITY_50 /* Zero coin probability */};

  TestRapporService rappor_service;
  rappor_service.SetCohortForTesting(0);
  rappor_service.SetSecretForTesting(
      HmacByteVectorGenerator::GenerateEntropyInput());

  rappor_service.TestRecordSample("MyMetric", kTestRapporParameters, "foo");
  rappor_service.TestRecordSample("MyMetric", kTestRapporParameters, "bar");

  RapporReports reports;
  rappor_service.GetReports(&reports);
  EXPECT_EQ(1, reports.report_size());

  const RapporReports::Report& report = reports.report(0);
  EXPECT_TRUE(report.name_hash());
  EXPECT_EQ(16u, report.bits().size());
}

}  // namespace rappor
