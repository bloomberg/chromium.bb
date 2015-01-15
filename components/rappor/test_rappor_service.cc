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

int TestRapporService::GetReportsCount() {
  RapporReports reports;
  ExportMetrics(&reports);
  return reports.report_size();
}

void TestRapporService::GetReports(RapporReports* reports) {
  ExportMetrics(reports);
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
