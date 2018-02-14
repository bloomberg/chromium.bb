// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_FAKE_CRYPTAUTH_ENROLLMENT_MANAGER_H_
#define COMPONENTS_CRYPTAUTH_FAKE_CRYPTAUTH_ENROLLMENT_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace cryptauth {

// Test double for CryptAuthEnrollmentManager.
class FakeCryptAuthEnrollmentManager : public CryptAuthEnrollmentManager {
 public:
  FakeCryptAuthEnrollmentManager();
  ~FakeCryptAuthEnrollmentManager() override;

  void set_time_to_next_attempt(const base::TimeDelta& time_to_next_attempt) {
    time_to_next_attempt_ = time_to_next_attempt;
  }

  void set_is_enrollment_valid(bool is_enrollment_valid) {
    is_enrollment_valid_ = is_enrollment_valid;
  }

  void set_user_public_key(const std::string& user_public_key) {
    user_public_key_ = user_public_key;
  }

  void set_user_private_key(const std::string& user_private_key) {
    user_private_key_ = user_private_key;
  }

  bool has_started() { return has_started_; }

  base::Optional<InvocationReason> last_invocation_reason() {
    return last_invocation_reason_;
  }

  // Finishes the active enrollment; should only be called if enrollment is in
  // progress due to a previous call to ForceEnrollmentNow(). If |success| is
  // true, |enrollment_finish_time| will be stored as the last enrollment time
  // and will be returned by future calls to GetLastEnrollmentTime().
  void FinishActiveEnrollment(
      bool success,
      const base::Time& enrollment_finish_time = base::Time());

  // Make these functions public for testing.
  using CryptAuthEnrollmentManager::NotifyEnrollmentStarted;
  using CryptAuthEnrollmentManager::NotifyEnrollmentFinished;

  // CryptAuthEnrollmentManager:
  void Start() override;
  void ForceEnrollmentNow(InvocationReason invocation_reason) override;
  bool IsEnrollmentValid() const override;
  base::Time GetLastEnrollmentTime() const override;
  base::TimeDelta GetTimeToNextAttempt() const override;
  bool IsEnrollmentInProgress() const override;
  bool IsRecoveringFromFailure() const override;
  std::string GetUserPublicKey() const override;
  std::string GetUserPrivateKey() const override;

 private:
  bool has_started_ = false;
  bool is_enrollment_in_progress_ = false;
  bool is_recovering_from_failure_ = false;
  bool is_enrollment_valid_ = false;
  base::Optional<InvocationReason> last_invocation_reason_;
  base::Time last_enrollment_time_;
  base::TimeDelta time_to_next_attempt_;
  std::string user_public_key_;
  std::string user_private_key_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthEnrollmentManager);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_FAKE_CRYPTAUTH_ENROLLMENT_MANAGER_H_
