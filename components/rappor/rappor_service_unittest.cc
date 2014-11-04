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
  TestRapporService(ReportingLevel reporting_level) : RapporService(&prefs) {
    RegisterPrefs(prefs.registry());
    Initialize(0,
               HmacByteVectorGenerator::GenerateEntropyInput(),
               reporting_level);
  }

  void GetReports(RapporReports* reports) {
    ExportMetrics(reports);
  }

  int32_t TestLoadCohort() {
    return LoadCohort();
  }

  std::string TestLoadSecret() {
    return LoadSecret();
  }

  void TestRecordSample(const std::string& metric_name,
                        const RapporParameters& parameters,
                        const std::string& sample) {
    RecordSampleInternal(metric_name, parameters, sample);
  }

  TestingPrefServiceSimple prefs;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRapporService);
};

TEST(RapporServiceTest, LoadCohort) {
  TestRapporService rappor_service(REPORTING_DISABLED);
  rappor_service.prefs.SetInteger(prefs::kRapporCohortSeed, 1);
  EXPECT_EQ(1, rappor_service.TestLoadCohort());
}

TEST(RapporServiceTest, LoadSecret) {
  TestRapporService rappor_service(REPORTING_DISABLED);
  std::string secret = HmacByteVectorGenerator::GenerateEntropyInput();
  std::string secret_base64;
  base::Base64Encode(secret, &secret_base64);
  rappor_service.prefs.SetString(prefs::kRapporSecret, secret_base64);
  EXPECT_EQ(secret, rappor_service.TestLoadSecret());
}

// Check that samples can be recorded and exported.
TEST(RapporServiceTest, RecordAndExportMetrics) {
  const RapporParameters kTestRapporParameters = {
      1 /* Num cohorts */,
      16 /* Bloom filter size bytes */,
      4 /* Bloom filter hash count */,
      PROBABILITY_75 /* Fake data probability */,
      PROBABILITY_50 /* Fake one probability */,
      PROBABILITY_75 /* One coin probability */,
      PROBABILITY_50 /* Zero coin probability */,
      COARSE_LEVEL};

  TestRapporService rappor_service(COARSE_LEVEL);

  // Multiple samples for the same metric should only generate one report.
  rappor_service.TestRecordSample("MyMetric", kTestRapporParameters, "foo");
  rappor_service.TestRecordSample("MyMetric", kTestRapporParameters, "bar");

  RapporReports reports;
  rappor_service.GetReports(&reports);
  EXPECT_EQ(1, reports.report_size());

  const RapporReports::Report& report = reports.report(0);
  EXPECT_TRUE(report.name_hash());
  EXPECT_EQ(16u, report.bits().size());
}

// Check that the reporting level is respected.
TEST(RapporServiceTest, ReportingLevel) {
  const RapporParameters kFineRapporParameters = {
      1 /* Num cohorts */,
      16 /* Bloom filter size bytes */,
      4 /* Bloom filter hash count */,
      PROBABILITY_75 /* Fake data probability */,
      PROBABILITY_50 /* Fake one probability */,
      PROBABILITY_75 /* One coin probability */,
      PROBABILITY_50 /* Zero coin probability */,
      FINE_LEVEL};

  TestRapporService rappor_service(COARSE_LEVEL);

  rappor_service.TestRecordSample("FineMetric", kFineRapporParameters, "foo");

  RapporReports reports;
  rappor_service.GetReports(&reports);
  EXPECT_EQ(0, reports.report_size());
}

}  // namespace rappor
