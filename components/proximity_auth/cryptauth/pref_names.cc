// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/pref_names.h"

namespace proximity_auth {
namespace prefs {

// The timestamp of the last successfull CryptAuth enrollment in seconds.
const char kCryptAuthEnrollmentLastEnrollmentTimeSeconds[] =
    "cryptauth.enrollment.last_enrollment_time_seconds";

// The reason that the next enrollment is performed. This should be one of the
// enum values of InvocationReason in
// components/proximity_auth/cryptauth/proto/cryptauth_api.proto.
extern const char kCryptAuthEnrollmentReason[] = "cryptauth.enrollment.reason";

// Whether the system is scheduling enrollments more aggressively to recover
// from the previous enrollment failure.
const char kCryptAuthEnrollmentIsRecoveringFromFailure[] =
    "cryptauth.enrollment.is_recovering_from_failure";

}  // namespace prefs
}  // proximity_auth
