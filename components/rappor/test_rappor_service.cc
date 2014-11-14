// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/test_rappor_service.h"

#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/rappor_parameters.h"

namespace rappor {

namespace {

bool MockIsIncognito() {
  return false;
}

}  // namespace

TestRapporService::TestRapporService()
    : RapporService(&prefs_, base::Bind(&MockIsIncognito)) {
  Initialize(0,
             HmacByteVectorGenerator::GenerateEntropyInput(),
             FINE_LEVEL);
}

TestRapporService::~TestRapporService() {}

int TestRapporService::GetReportsCount() {
  RapporReports reports;
  ExportMetrics(&reports);
  return reports.report_size();
}

}  // namespace rappor
