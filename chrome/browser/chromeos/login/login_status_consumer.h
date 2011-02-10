// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_STATUS_CONSUMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_STATUS_CONSUMER_H_
#pragma once

#include <string>

#include "base/logging.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"

namespace chromeos {

class LoginFailure {
 public:
  enum FailureReason {
    NONE,
    COULD_NOT_MOUNT_CRYPTOHOME,
    COULD_NOT_MOUNT_TMPFS,
    COULD_NOT_UNMOUNT_CRYPTOHOME,
    DATA_REMOVAL_FAILED,  // Could not destroy your old data
    LOGIN_TIMED_OUT,
    UNLOCK_FAILED,
    NETWORK_AUTH_FAILED,  // Could not authenticate against Google
    NUM_FAILURE_REASONS,  // This has to be the last item.
  };

  explicit LoginFailure(FailureReason reason)
      : reason_(reason),
        error_(GoogleServiceAuthError::NONE) {
    DCHECK(reason != NETWORK_AUTH_FAILED);
  }

  inline bool operator==(const LoginFailure &b) const {
    if (reason_ != b.reason_) {
      return false;
    }
    if (reason_ == NETWORK_AUTH_FAILED) {
      return error_ == b.error_;
    }
    return true;
  }

  static LoginFailure FromNetworkAuthFailure(
      const GoogleServiceAuthError& error) {
    return LoginFailure(NETWORK_AUTH_FAILED, error);
  }

  static LoginFailure None() {
    return LoginFailure(NONE);
  }

  const std::string GetErrorString() const {
    switch (reason_) {
      case DATA_REMOVAL_FAILED:
        return "Could not destroy your old data.";
      case COULD_NOT_MOUNT_CRYPTOHOME:
        return "Could not mount cryptohome.";
      case COULD_NOT_UNMOUNT_CRYPTOHOME:
        return "Could not mount cryptohome.";
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
      default:
        NOTREACHED();
        return std::string();
    }
  }

  const GoogleServiceAuthError& error() const { return error_; }
  const FailureReason& reason() const { return reason_; }

 private:
  LoginFailure(FailureReason reason, GoogleServiceAuthError error)
      : reason_(reason),
        error_(error) {
  }

  FailureReason reason_;
  GoogleServiceAuthError error_;
};

// An interface that defines the callbacks for objects that the
// Authenticator class will call to report the success/failure of
// authentication for Chromium OS.
class LoginStatusConsumer {
 public:
  virtual ~LoginStatusConsumer() {}
  // The current login attempt has ended in failure, with error |error|.
  virtual void OnLoginFailure(const LoginFailure& error) = 0;
  // The current login attempt has succeeded for
  // |username|/|password|, returning |credentials|.  If
  // |pending_requests| is false, we're totally done.  If it's true,
  // we will still have some more results to report later.
  virtual void OnLoginSuccess(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests) = 0;
  // The current guest login attempt has succeeded.
  virtual void OnOffTheRecordLoginSuccess() {}
  // The same password didn't work both online and offline.
  virtual void OnPasswordChangeDetected(
      const GaiaAuthConsumer::ClientLoginResult& credentials) {
    NOTREACHED();
  };
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_STATUS_CONSUMER_H_
