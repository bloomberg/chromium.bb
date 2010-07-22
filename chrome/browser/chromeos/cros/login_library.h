// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_LOGIN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_LOGIN_LIBRARY_H_

#include <string>

#include "base/singleton.h"
#include "cros/chromeos_login.h"

namespace chromeos {

// This interface defines the interaction with the ChromeOS login library APIs.
class LoginLibrary {
 public:
  virtual ~LoginLibrary() {}
  // Requests that the Upstart signal login-prompt-ready be emitted.
  virtual bool EmitLoginPromptReady() = 0;

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
};

// This class handles the interaction with the ChromeOS login library APIs.
class LoginLibraryImpl : public LoginLibrary {
 public:
  LoginLibraryImpl() {}
  virtual ~LoginLibraryImpl() {}

  // LoginLibrary overrides.
  virtual bool EmitLoginPromptReady();
  virtual bool StartSession(const std::string& user_email,
                            const std::string& unique_id /* unused */);
  virtual bool StopSession(const std::string& unique_id /* unused */);

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_LOGIN_LIBRARY_H_
