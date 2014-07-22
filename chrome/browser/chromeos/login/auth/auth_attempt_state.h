// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_AUTH_ATTEMPT_STATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_AUTH_ATTEMPT_STATE_H_

#include <string>

#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// Tracks the state associated with a single attempt to log in to chromium OS.
// Enforces that methods are only called on the UI thread.
class AuthAttemptState {
 public:
  // Used to initialize for a login attempt.
  AuthAttemptState(const UserContext& user_context,
                   user_manager::UserType user_type,
                   bool unlock,
                   bool online_complete,
                   bool user_is_new);

  virtual ~AuthAttemptState();

  // Copy |user_context| and copy |outcome| into this object, so we can have
  // a copy we're sure to own, and can make available on the UI thread.
  // Must be called from the UI thread.
  void RecordOnlineLoginStatus(const AuthFailure& outcome);

  // Copy |username_hash| into this object, so we can have
  // a copy we're sure to own, and can make available on the UI thread.
  // Must be called from the UI thread.
  void RecordUsernameHash(const std::string& username_hash);

  // Marks that the username hash request attempt has failed.
  void RecordUsernameHashFailed();

  // Marks username hash as being requested so that flow will block till both
  // requests (Mount/GetUsernameHash) are completed.
  void UsernameHashRequested();

  // The next attempt will not allow HOSTED accounts to log in.
  void DisableHosted();

  // Copy |cryptohome_code| and |cryptohome_outcome| into this object,
  // so we can have a copy we're sure to own, and can make available
  // on the UI thread.  Must be called from the UI thread.
  void RecordCryptohomeStatus(bool cryptohome_outcome,
                              cryptohome::MountError cryptohome_code);

  // Blow away locally stored cryptohome login status.
  // Must be called from the UI thread.
  void ResetCryptohomeStatus();

  virtual bool online_complete();
  virtual const AuthFailure& online_outcome();
  virtual bool is_first_time_user();
  virtual GaiaAuthFetcher::HostedAccountsSetting hosted_policy();

  virtual bool cryptohome_complete();
  virtual bool cryptohome_outcome();
  virtual cryptohome::MountError cryptohome_code();

  virtual bool username_hash_obtained();
  virtual bool username_hash_valid();

  // Saved so we can retry client login, and also so we know for whom login
  // has succeeded, in the event of successful completion.
  UserContext user_context;

  // These fields are saved so we can retry client login.
  const std::string login_token;
  const std::string login_captcha;

  // The type of the user attempting to log in.
  const user_manager::UserType user_type;

  const bool unlock;  // True if authenticating to unlock the computer.

 protected:
  // Status of our online login attempt.
  bool online_complete_;
  AuthFailure online_outcome_;

  // Whether or not we're accepting HOSTED accounts during the current
  // online auth attempt.
  GaiaAuthFetcher::HostedAccountsSetting hosted_policy_;
  bool is_first_time_user_;

  // Status of our cryptohome op attempt. Can only have one in flight at a time.
  bool cryptohome_complete_;
  bool cryptohome_outcome_;
  cryptohome::MountError cryptohome_code_;

 private:
  // Status of the crypthome GetSanitizedUsername() async call.
  // This gets initialized as being completed and those callers
  // that would explicitly request username hash would have to reset this.
  bool username_hash_obtained_;

  // After the username hash request is completed, this marks whether
  // the request was successful.
  bool username_hash_valid_;

  DISALLOW_COPY_AND_ASSIGN(AuthAttemptState);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_AUTH_ATTEMPT_STATE_H_
