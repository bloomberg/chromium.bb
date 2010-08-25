// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_LOGIN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_LOGIN_LIBRARY_H_
#pragma once

#include <string>

#include "base/singleton.h"
#include "cros/chromeos_login.h"

namespace chromeos {

// This interface defines the interaction with the ChromeOS login library APIs.
class LoginLibrary {
 public:
  template <class T>
  class Delegate {
   public:
    virtual void Run(T value) = 0;
  };

  virtual ~LoginLibrary() {}
  // Requests that the Upstart signal login-prompt-ready be emitted.
  virtual bool EmitLoginPromptReady() = 0;

  // Attempts to asynchronously set the provided public key as the
  // Owner's public key for this device.  |public_key_der| should be a
  // DER-encoded PKCS11 SubjectPublicKeyInfo structure.
  //  Returns true if the attempt was successfully started.
  //  callback->Run() will be called when the operation is complete.
  virtual bool SetOwnerKey(const std::vector<uint8>& public_key_der,
                           Delegate<bool>* callback) = 0;

  // Tells the session manager to start a logged-in session for the user
  // |user_email|.  |unique_id| is meant to be used when we have a non-human-
  // readable unique identifier by which we distinguish users (to deal with
  // potential email address changes over time).
  virtual bool StartSession(const std::string& user_email,
                            const std::string& unique_id /* unused */) = 0;

  // Tells the session manager to terminate the current logged-in session.
  // In the event that we ever support multiple simultaneous user sessions,
  // This will tell the session manager to terminate the session for the user
  // indicated by |unique_id|.
  virtual bool StopSession(const std::string& unique_id /* unused */) = 0;

  // Restarts the job with specified command line string.
  virtual bool RestartJob(int pid, const std::string& command_line) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static LoginLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_LOGIN_LIBRARY_H_
