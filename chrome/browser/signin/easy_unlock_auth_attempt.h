// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_AUTH_ATTEMPT_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_AUTH_ATTEMPT_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"

class EasyUnlockAppManager;

// Class responsible for handling easy unlock auth attempts (both for unlocking
// the screen and logging in). The auth protocol is started by calling |Start|,
// which notifies the easy unlock app about auth attempt. When the auth result
// is available, |FinalizeUnlock| or |FinalizeSignin| should be called,
// depending on auth type.
// To cancel the in progress auth attempt, delete the |EasyUnlockAuthAttempt|
// object.
class EasyUnlockAuthAttempt {
 public:
  // The auth type.
  enum Type {
    TYPE_UNLOCK,
    TYPE_SIGNIN
  };

  // A callback to be invoked after the auth attempt is finalized. |success|
  // indicates whether the attempt is successful or not. |user_id| is the
  // associated user. |key_secret| is the user secret for a sign-in attempt
  // and |key_label| is the label of the corresponding cryptohome key.
  typedef base::Callback<void(Type auth_attempt_type,
                              bool success,
                              const std::string& user_id,
                              const std::string& key_secret,
                              const std::string& key_label)>
      FinalizedCallback;

  EasyUnlockAuthAttempt(EasyUnlockAppManager* app_manager,
                        const std::string& user_id,
                        Type type,
                        const FinalizedCallback& finalized_callback);
  ~EasyUnlockAuthAttempt();

  // Starts the auth attempt by sending screenlockPrivate.onAuthAttempted event
  // to easy unlock app. Returns whether the event was successfully dispatched.
  bool Start();

  // Finalizes an unlock attempt. It unlocks the screen if |success| is true.
  // If |this| has TYPE_SIGNIN type, calling this method will cause signin
  // failure equivalent to cancelling the attempt.
  void FinalizeUnlock(const std::string& user_id, bool success);

  // Finalizes signin attempt. It tries to log in using the secret derived from
  // |wrapped_secret| decrypted by |session_key|. If the decryption fails, it
  // fails the signin attempt.
  // If called on an object with TYPE_UNLOCK type, it will cause unlock failure
  // equivalent to cancelling the request.
  void FinalizeSignin(const std::string& user_id,
                      const std::string& wrapped_secret,
                      const std::string& session_key);

 private:
  // The internal attempt state.
  enum State {
    STATE_IDLE,
    STATE_RUNNING,
    STATE_DONE
  };

  // Cancels the attempt.
  void Cancel(const std::string& user_id);

  EasyUnlockAppManager* app_manager_;
  State state_;
  std::string user_id_;
  Type type_;

  FinalizedCallback finalized_callback_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockAuthAttempt);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_AUTH_ATTEMPT_H_
