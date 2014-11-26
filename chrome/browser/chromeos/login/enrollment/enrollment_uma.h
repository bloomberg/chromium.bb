// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_UMA_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_UMA_H_

#include "chrome/browser/chromeos/login/enrollment/enrollment_mode.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"

namespace chromeos {

// Logs an UMA |event| in "Enrollment.*" histogram. Histogram is choosen
// depending on |mode|.
void EnrollmentUMA(policy::MetricEnrollment sample, EnrollmentMode mode);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_UMA_H_
