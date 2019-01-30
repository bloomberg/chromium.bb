// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_enrollment_scheduler.h"

namespace chromeos {

namespace device_sync {

CryptAuthEnrollmentScheduler::CryptAuthEnrollmentScheduler(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

CryptAuthEnrollmentScheduler::~CryptAuthEnrollmentScheduler() = default;

void CryptAuthEnrollmentScheduler::NotifyEnrollmentRequested(
    const base::Optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference) const {
  delegate_->OnEnrollmentRequested(client_directive_policy_reference);
}

}  // namespace device_sync

}  // namespace chromeos
