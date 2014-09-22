// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/rappor_service.h"

#include "base/base64.h"
#include "base/prefs/testing_pref_service.h"
#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/rappor_parameters.h"
#include "components/rappor/rappor_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace rappor {

class TestRapporService : public RapporService {
 public:
  TestRapporService() : RapporService(&prefs_) {
    RegisterPrefs(prefs_.registry());
    prefs_.SetInteger(prefs::kRapporCohortSeed, 0);
    std::string secret = HmacByteVectorGenerator::GenerateEntropyInput();
    std::string secret_base64;
    base::Base64Encode(secret, &secret_base64);
    prefs_.SetString(prefs::kRapporSecret, secret_base64);
    LoadCohort();
    LoadSecret();
  }

  void GetReports(RapporReports* reports) {
    ExportMetrics(reports);
  }

  void TestRecordSample(const std::string& metric_name,
                        const RapporParameters& parameters,
                        const std::string& sample) {
    RecordSampleInternal(metric_name, parameters, sample);
  }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRapporService);
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
