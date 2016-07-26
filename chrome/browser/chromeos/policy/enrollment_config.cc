// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/enrollment_config.h"

namespace policy {

// TODO(drcrash): Initialize in declarations and get rid of constructor.
EnrollmentConfig::EnrollmentConfig()
    : mode(MODE_NONE),
      // TODO(drcrash): Change to best available once ZTE is everywhere.
      auth_mechanism(AUTH_MECHANISM_INTERACTIVE) {}

}  // namespace policy
