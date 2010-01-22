// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LIBRARY_H_

#include <string>

#include "base/singleton.h"
#include "third_party/cros/chromeos_login.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS login library APIs.
// Users can get an instance of this library class like this:
//   LoginLibrary::Get()
class LoginLibrary {
 public:
  // If the libray fails to load the first time we check, we don't want to
  // keep trying to load the library.  If it fails the first time, it fails.
  static bool tried_and_failed;

  // This gets the singleton LoginLibrary.
  static LoginLibrary* Get();

  // Makes sure the library is loaded, loading it if necessary. Returns true if
  // the library has been successfully loaded.
  static bool EnsureLoaded();

  // Requests that the Upstart signal login-prompt-ready be emitted.
  bool EmitLoginPromptReady();

  // Tells the session manager to start a logged-in session for the user
  // |user_email|.  |unique_id| is meant to be used when we have a non-human-
  // readable unique identifier by which we distinguish users (to deal with
  // potential email address changes over time).
  bool StartSession(const std::string& user_email,
                    const std::string& unique_id /* unused */);

  // Tells the session manager to terminate the current logged-in session.
  // In the event that we ever support multiple simultaneous user sessions,
  // This will tell the session manager to terminate the session for the user
  // indicated by |unique_id|.
  bool StopSession(const std::string& unique_id /* unused */);

 private:
  friend struct DefaultSingletonTraits<LoginLibrary>;

  LoginLibrary() {}
  ~LoginLibrary() {}

  DISALLOW_COPY_AND_ASSIGN(LoginLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LIBRARY_H_
