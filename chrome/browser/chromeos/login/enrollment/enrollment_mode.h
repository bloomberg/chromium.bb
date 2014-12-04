// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_MODE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_MODE_H_

#include <string>

namespace chromeos {

// Describes the enrollment mode.  Must be kept in sync with
// |kEnrollmentModes| in enrollment_mode.cc.
enum EnrollmentMode {
  ENROLLMENT_MODE_MANUAL,    // Manually triggered enrollment.
  ENROLLMENT_MODE_FORCED,    // Forced enrollment, user can't skip.
  ENROLLMENT_MODE_RECOVERY,  // Recover from "spontaneous unenrollment".
  ENROLLMENT_MODE_COUNT      // Counter must be last. Not an enrollment mode.
};

// Converts |mode| to a human-readable string.
std::string EnrollmentModeToString(EnrollmentMode mode);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_MODE_H_
