// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_

#include <string>

#include "base/memory/ref_counted.h"

class GURL;
class PrefService;
class Profile;

namespace base {
class CommandLine;
}

namespace chromeos {

class Authenticator;
class LoginDisplayHost;
class AuthStatusConsumer;
class UserContext;

class LoginUtils {
 public:
  class Delegate {
   public:
    // Called after profile is loaded and prepared for the session.
    virtual void OnProfilePrepared(Profile* profile) = 0;

#if defined(ENABLE_RLZ)
    // Called after post-profile RLZ initialization.
    virtual void OnRlzInitialized() {}
#endif
   protected:
    virtual ~Delegate() {}
  };

  // Get LoginUtils singleton object. If it was not set before, new default
  // instance will be created.
  static LoginUtils* Get();

  // Set LoginUtils singleton object for test purpose only!
  static void Set(LoginUtils* ptr);

  // Checks if the given username is whitelisted and allowed to sign-in to
  // this device. |wildcard_match| may be NULL. If it's present, it'll be set to
  // true if the whitelist check was satisfied via a wildcard.
  static bool IsWhitelisted(const std::string& username, bool* wildcard_match);

  virtual ~LoginUtils() {}

  // Thin wrapper around StartupBrowserCreator::LaunchBrowser().  Meant to be
  // used in a Task posted to the UI thread.  Once the browser is launched the
  // login host is deleted.
  virtual void DoBrowserLaunch(Profile* profile,
                               LoginDisplayHost* login_host) = 0;

  // Loads and prepares profile for the session. Fires |delegate| in the end.
  // |user_context.username_hash| defines when user homedir is mounted.
  // Also see DelegateDeleted method.
  // If |has_active_session| is true than this is a case of restoring user
  // session after browser crash so no need to start new session.
  virtual void PrepareProfile(
      const UserContext& user_context,
      bool has_auth_cookies,
      bool has_active_session,
      Delegate* delegate) = 0;

  // Invalidates |delegate|, which was passed to PrepareProfile method call.
  virtual void DelegateDeleted(Delegate* delegate) = 0;

  // Invoked after the tmpfs is successfully mounted.
  // Asks session manager to restart Chrome in Browse Without Sign In mode.
  // |start_url| is url for launched browser to open.
  virtual void CompleteOffTheRecordLogin(const GURL& start_url) = 0;

  // Creates and returns the authenticator to use.
  // Before WebUI login (Up to R14) the caller owned the returned
  // Authenticator instance and had to delete it when done.
  // New instance was created on each new login attempt.
  // Starting with WebUI login (R15) single Authenticator instance is used for
  // entire login process, even for multiple retries. Authenticator instance
  // holds reference to login profile and is later used during fetching of
  // OAuth tokens.
  // TODO(nkostylev): Cleanup after WebUI login migration is complete.
  virtual scoped_refptr<Authenticator> CreateAuthenticator(
      AuthStatusConsumer* consumer) = 0;

  // Initiates process restart if needed.
  // |early_restart| is true if this restart attempt happens before user profile
  // is fully initialized.
  // Might not return if restart is possible right now.
  // Returns true if restart was scheduled.
  // Returns false if no restart is needed,
  virtual bool RestartToApplyPerSessionFlagsIfNeed(Profile* profile,
                                                   bool early_restart) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
