// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_scheduler.h"

namespace chromeos {

namespace device_sync {

CryptAuthScheduler::CryptAuthScheduler() = default;

CryptAuthScheduler::~CryptAuthScheduler() = default;

void CryptAuthScheduler::StartEnrollmentScheduling(
    const base::WeakPtr<EnrollmentDelegate>& enrollment_delegate) {
  // Ensure this is only called once.
  DCHECK(!enrollment_delegate_);

  DCHECK(enrollment_delegate);
  enrollment_delegate_ = enrollment_delegate;

  OnEnrollmentSchedulingStarted();
}

bool CryptAuthScheduler::HasEnrollmentSchedulingStarted() {
  return enrollment_delegate_.get();
}

void CryptAuthScheduler::NotifyEnrollmentRequested(
    const cryptauthv2::ClientMetadata& client_metadata,
    const base::Optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference) const {
  // Do nothing if weak pointer was invalidated.
  if (!enrollment_delegate_)
    return;

  enrollment_delegate_->OnEnrollmentRequested(
      client_metadata, client_directive_policy_reference);
}

void CryptAuthScheduler::OnEnrollmentSchedulingStarted() {}

}  // namespace device_sync

}  // namespace chromeos
