// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_ENROLLER_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_ENROLLER_H

#include <string>

#include "base/callback_forward.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"

namespace proximity_auth {

// Interface for enrolling a device with CryptAuth.
class CryptAuthEnroller {
 public:
  // Enrolls the device with |device_info| properties for a given
  // |invocation_reason|. |callback| will be called with true if the enrollment
  // succeeds and false otherwise.
  typedef base::Callback<void(bool)> EnrollmentFinishedCallback;
  virtual void Enroll(const cryptauth::GcmDeviceInfo& device_info,
                      cryptauth::InvocationReason invocation_reason,
                      const EnrollmentFinishedCallback& callback) = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_ENROLLER_H
