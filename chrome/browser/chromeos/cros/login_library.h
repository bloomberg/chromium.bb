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
  class Delegate {
   public:
    virtual void OnComplete(bool value) = 0;
  };

  virtual ~LoginLibrary() {}
  // Requests that the Upstart signal login-prompt-ready be emitted.
  virtual bool EmitLoginPromptReady() = 0;

  // Check whether or not |email| is present on the whitelist.
  // If so, we return true and store the signature passed when |email| was
  // whitelisted in |OUT_signature|.
  // If not, we return false and don't touch the output parameter.
  virtual bool CheckWhitelist(const std::string& email,
                              std::vector<uint8>* OUT_signature) = 0;

  virtual void RequestRetrievePolicy(RetrievePolicyCallback callback,
                                     void* delegate_string) = 0;

  // Start fetch the value associated with |name|, if its present.
  // When fetching is done/failed, |callback| is called to pass back the fetch
  // results. If fetching is successful, |callback| will be called with
  // true for |success| and property's name, value and signature filled in
  // Property struct. Otherwise, |success| would be false.
  virtual void RequestRetrieveProperty(const std::string& name,
                                       RetrievePropertyCallback callback,
                                       void* user_data) = 0;

  virtual void RequestStorePolicy(const std::string& policy,
                                  StorePolicyCallback callback,
                                  void* delegate_bool) = 0;

  // Attempts to issue a signed async request to store |name|=|value|.
  // |signature| must by a SHA1 with RSA encryption signature over the string
  // "name=value" with the owner's private key.
  //  Returns true if the attempt was successfully started.
  //  callback->Run() will be called when the operation is complete.
  virtual bool StorePropertyAsync(const std::string& name,
                                  const std::string& value,
                                  const std::vector<uint8>& signature,
                                  Delegate* callback) = 0;

  // Attempts to issue a signed async request to whitelist |email|.
  // |signature| must by a SHA1 with RSA encryption signature over |email|
  // with the owner's private key.
  //  Returns true if the attempt was successfully started.
  //  callback->Run() will be called when the operation is complete.
  virtual bool WhitelistAsync(const std::string& email,
                              const std::vector<uint8>& signature,
                              Delegate* callback) = 0;

  // Attempts to issue a signed async request to remove |email| from the
  // whitelist of users allowed to log in to this machine.
  // |signature| must by a SHA1 with RSA encryption signature over |email|
  // with the owner's private key.
  //  Returns true if the attempt was successfully started.
  //  callback->Run() will be called when the operation is complete.
  virtual bool UnwhitelistAsync(const std::string& email,
                                const std::vector<uint8>& signature,
                                Delegate* callback) = 0;

  // DEPRECATED.  We have re-implemented owner-signed settings by fetching
  // and caching a policy, and then pulling values from there.  This is all
  // handled at the SignedSettings layer, so anyone using this stuff directly
  // should not be doing so anymore.
  //
  // Retrieves the user white list. Note the call is for display purpose only.
  // To determine if an email is white listed, you MUST use CheckWhitelist.
  //  Returns true if the request is successfully dispatched.
  virtual bool EnumerateWhitelisted(std::vector<std::string>* whitelisted) = 0;

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
