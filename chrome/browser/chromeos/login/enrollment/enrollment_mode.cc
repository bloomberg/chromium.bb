// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enrollment_mode.h"

#include "base/logging.h"

namespace {

// Enrollment mode strings.
const char* const kModeStrings[chromeos::ENROLLMENT_MODE_COUNT] =
    {"manual", "forced", "recovery"};

}  // namespace

namespace chromeos {

// static
std::string EnrollmentModeToString(EnrollmentMode mode) {
  CHECK_LE(0, mode);
  CHECK_LT(mode, ENROLLMENT_MODE_COUNT);
  return kModeStrings[mode];
}

}  // namespace chromeos
