// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_LOGIN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_LOGIN_LIBRARY_H_
#pragma once

#include <string>

#include "base/memory/singleton.h"
#include "third_party/cros/chromeos_login.h"

namespace chromeos {

// This interface defines the interaction with the ChromeOS login library APIs.
class LoginLibrary {
 public:
  virtual ~LoginLibrary();

  virtual void Init() = 0;

  // Requests that the Upstart signal login-prompt-ready be emitted.
  virtual void EmitLoginPromptReady() = 0;

  // Requests that the Upstart signal login-prompt-visible be emitted. This is
  // currently used only with views-desktop or aura environments. In the normal
  // environment, the chromeos-wm emits the signal.
  virtual void EmitLoginPromptVisible() = 0;

  virtual void RequestRetrievePolicy(RetrievePolicyCallback callback,
                                     void* delegate_string) = 0;

  virtual void RequestStorePolicy(const std::string& policy,
                                  StorePolicyCallback callback,
                                  void* delegate_bool) = 0;

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

  // Restarts the Enterprise Daemon.
  virtual bool RestartEntd() = 0;

  // Restarts the job with specified command line string.
  virtual bool RestartJob(int pid, const std::string& command_line) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static LoginLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_LOGIN_LIBRARY_H_
