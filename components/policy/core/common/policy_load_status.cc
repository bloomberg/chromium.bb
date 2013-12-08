// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_load_status.h"

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "components/policy/core/common/policy_types.h"

namespace policy {

namespace {

const char kHistogramName[] = "Enterprise.PolicyLoadStatus";

}  // namespace

PolicyLoadStatusSample::PolicyLoadStatusSample()
    : histogram_(base::LinearHistogram::FactoryGet(
          kHistogramName, 1, POLICY_LOAD_STATUS_SIZE,
          POLICY_LOAD_STATUS_SIZE + 1,
          base::Histogram::kUmaTargetedHistogramFlag)) {
  Add(POLICY_LOAD_STATUS_STARTED);
}

PolicyLoadStatusSample::~PolicyLoadStatusSample() {
  for (int i = 0; i < POLICY_LOAD_STATUS_SIZE; ++i) {
    if (status_bits_[i])
      histogram_->Add(i);
  }
}

void PolicyLoadStatusSample::Add(PolicyLoadStatus status) {
  status_bits_[status] = true;
}

}  // namespace policy
