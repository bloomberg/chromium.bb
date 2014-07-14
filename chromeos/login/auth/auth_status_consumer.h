// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_AUTH_STATUS_CONSUMER_H_
#define CHROMEOS_LOGIN_AUTH_AUTH_STATUS_CONSUMER_H_

#include <string>

#include "base/logging.h"
#include "chromeos/chromeos_export.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"

namespace chromeos {

class UserContext;

class CHROMEOS_EXPORT AuthFailure {
 public:
  enum FailureReason {
    NONE,
    COULD_NOT_MOUNT_CRYPTOHOME,
    COULD_NOT_MOUNT_TMPFS,
    COULD_NOT_UNMOUNT_CRYPTOHOME,
    DATA_REMOVAL_FAILED,  // Could not destroy your old data
    LOGIN_TIMED_OUT,
    UNLOCK_FAILED,
    NETWORK_AUTH_FAILED,     // Could not authenticate against Google
    OWNER_REQUIRED,          // Only the device owner can log-in.
    WHITELIST_CHECK_FAILED,  // Login attempt blocked by whitelist. This value
                             // is
                             // synthesized by the ExistingUserController and
                             // passed to the login_status_consumer_ in tests
    // only. It is never generated or seen by any of the
    // other authenticator classes.
    TPM_ERROR,             // Critical TPM error encountered.
    USERNAME_HASH_FAILED,  // Could not get username hash.
    NUM_FAILURE_REASONS,   // This has to be the last item.
  };

  explicit AuthFailure(FailureReason reason)
      : reason_(reason), error_(GoogleServiceAuthError::NONE) {
    DCHECK(reason != NETWORK_AUTH_FAILED);
  }

  inline bool operator==(const AuthFailure& b) const {
    if (reason_ != b.reason_) {
      return false;
    }
    if (reason_ == NETWORK_AUTH_FAILED) {
      return error_ == b.error_;
    }
    return true;
  }

  static AuthFailure FromNetworkAuthFailure(
      const GoogleServiceAuthError& error) {
    return AuthFailure(NETWORK_AUTH_FAILED, error);
  }

  static AuthFailure AuthFailureNone() { return AuthFailure(NONE); }

  const std::string GetErrorString() const {
    switch (reason_) {
      case DATA_REMOVAL_FAILED:
        return "Could not destroy your old data.";
      case COULD_NOT_MOUNT_CRYPTOHOME:
        return "Could not mount cryptohome.";
      case COULD_NOT_UNMOUNT_CRYPTOHOME:
        return "Could not unmount cryptohome.";
      case COULD_NOT_MOUNT_TMPFS:
        return "Could not mount tmpfs.";
      case LOGIN_TIMED_OUT:
        return "Login timed out. Please try again.";
      case UNLOCK_FAILED:
        return "Unlock failed.";
      case NETWORK_AUTH_FAILED:
        if (error_.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
          return net::ErrorToString(error_.network_error());
        }
        return "Google authentication failed.";
      case OWNER_REQUIRED:
        return "Login is restricted to the owner's account only.";
      case WHITELIST_CHECK_FAILED:
        return "Login attempt blocked by whitelist.";
      default:
        NOTREACHED();
        return std::string();
    }
  }

  const GoogleServiceAuthError& error() const { return error_; }
  const FailureReason& reason() const { return reason_; }

 private:
  AuthFailure(FailureReason reason, GoogleServiceAuthError error)
      : reason_(reason), error_(error) {}

  FailureReason reason_;
  GoogleServiceAuthError error_;
};

// An interface that defines the callbacks for objects that the
// Authenticator class will call to report the success/failure of
// authentication for Chromium OS.
class CHROMEOS_EXPORT AuthStatusConsumer {
 public:
  virtual ~AuthStatusConsumer() {}
  // The current login attempt has ended in failure, with error |error|.
  virtual void OnAuthFailure(const AuthFailure& error) = 0;

  // The current retail mode login attempt has succeeded.
  // Unless overridden for special processing, this should always call
  // OnLoginSuccess with the magic |kRetailModeUserEMail| constant.
  virtual void OnRetailModeAuthSuccess(const UserContext& user_context);
  // The current login attempt has succeeded for |user_context|.
  virtual void OnAuthSuccess(const UserContext& user_context) = 0;
  // The current guest login attempt has succeeded.
  virtual void OnOffTheRecordAuthSuccess() {}
  // The same password didn't work both online and offline.
  virtual void OnPasswordChangeDetected();
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_AUTH_STATUS_CONSUMER_H_
