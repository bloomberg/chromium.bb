// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enrollment_uma.h"

#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"

namespace {

const char * const kMetricEnrollment = "Enterprise.Enrollment";
const char * const kMetricEnrollmentForced = "Enterprise.EnrollmentForced";
const char * const kMetricEnrollmentRecovery = "Enterprise.EnrollmentRecovery";

}  // namespace

namespace chromeos {

void EnrollmentUMA(policy::MetricEnrollment sample, EnrollmentMode mode) {
  switch (mode) {
    case ENROLLMENT_MODE_MANUAL:
      UMA_HISTOGRAM_SPARSE_SLOWLY(kMetricEnrollment, sample);
      break;
    case ENROLLMENT_MODE_FORCED:
      UMA_HISTOGRAM_SPARSE_SLOWLY(kMetricEnrollmentForced, sample);
      break;
    case ENROLLMENT_MODE_RECOVERY:
      UMA_HISTOGRAM_SPARSE_SLOWLY(kMetricEnrollmentRecovery, sample);
      break;
    case ENROLLMENT_MODE_COUNT:
      NOTREACHED();
      break;
  }
}

}  // namespace chromeos
