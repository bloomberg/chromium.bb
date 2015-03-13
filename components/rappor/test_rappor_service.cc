// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/test_rappor_service.h"

#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/rappor_parameters.h"
#include "components/rappor/rappor_prefs.h"
#include "components/rappor/test_log_uploader.h"

namespace rappor {

namespace {

bool MockIsIncognito(bool* is_incognito) {
  return *is_incognito;
}

}  // namespace

TestRapporService::TestRapporService()
    : RapporService(&test_prefs_, base::Bind(&MockIsIncognito, &is_incognito_)),
      next_rotation_(base::TimeDelta()),
      is_incognito_(false) {
  RegisterPrefs(test_prefs_.registry());
  test_uploader_ = new TestLogUploader();
  InitializeInternal(make_scoped_ptr(test_uploader_),
                     0,
                     HmacByteVectorGenerator::GenerateEntropyInput());
  Update(FINE_LEVEL, true);
}

TestRapporService::~TestRapporService() {}

void TestRapporService::RecordSample(const std::string& metric_name,
                                     RapporType type,
                                     const std::string& sample) {
  // Save the recorded sample to the local structure.
  RapporSample rappor_sample;
  rappor_sample.type = type;
  rappor_sample.value = sample;
  samples_[metric_name] = rappor_sample;
  // Original version is still called.
  RapporService::RecordSample(metric_name, type, sample);
}

int TestRapporService::GetReportsCount() {
  RapporReports reports;
  ExportMetrics(&reports);
  return reports.report_size();
}

void TestRapporService::GetReports(RapporReports* reports) {
  ExportMetrics(reports);
}

bool TestRapporService::GetRecordedSampleForMetric(
    const std::string& metric_name,
    std::string* sample,
    RapporType* type) {
  SamplesMap::iterator it = samples_.find(metric_name);
  if (it == samples_.end())
    return false;
  *sample = it->second.value;
  *type = it->second.type;
  return true;
}

// Cancel the next call to OnLogInterval.
void TestRapporService::CancelNextLogRotation() {
  next_rotation_ = base::TimeDelta();
}

// Schedule the next call to OnLogInterval.
void TestRapporService::ScheduleNextLogRotation(base::TimeDelta interval) {
  next_rotation_ = interval;
}

}  // namespace rappor
