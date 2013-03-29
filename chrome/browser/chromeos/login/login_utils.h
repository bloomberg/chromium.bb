// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_

#include <string>

#include "base/memory/ref_counted.h"

class CommandLine;
class GURL;
class Profile;
class PrefRegistrySimple;
class PrefService;

namespace chromeos {

class Authenticator;
class LoginDisplayHost;
class LoginStatusConsumer;
struct UserContext;

class LoginUtils {
 public:
  class Delegate {
   public:
    // Called after profile is loaded and prepared for the session.
    virtual void OnProfilePrepared(Profile* profile) = 0;

#if defined(ENABLE_RLZ)
    // Called after post-profile RLZ initialization.
    virtual void OnRlzInitialized(Profile* profile) {}
#endif
   protected:
    virtual ~Delegate() {}
  };

  // Registers log-in related preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Get LoginUtils singleton object. If it was not set before, new default
  // instance will be created.
  static LoginUtils* Get();

  // Set LoginUtils singleton object for test purpose only!
  static void Set(LoginUtils* ptr);

  // Checks if the given username is whitelisted and allowed to sign-in to
  // this device.
  static bool IsWhitelisted(const std::string& username);

  virtual ~LoginUtils() {}

  // Thin wrapper around StartupBrowserCreator::LaunchBrowser().  Meant to be
  // used in a Task posted to the UI thread.  Once the browser is launched the
  // login host is deleted.
  virtual void DoBrowserLaunch(Profile* profile,
                               LoginDisplayHost* login_host) = 0;

  // Loads and prepares profile for the session. Fires |delegate| in the end.
  // If |pending_requests| is true, there's a pending online auth request.
  // If |display_email| is not empty, user's displayed email will be set to
  // this value, shown in UI.
  // |user_context.username_hash| defines when user homedir is mounted.
  // Also see DelegateDeleted method.
  virtual void PrepareProfile(
      const UserContext& user_context,
      const std::string& display_email,
      bool using_oauth,
      bool has_cookies,
      Delegate* delegate) = 0;

  // Invalidates |delegate|, which was passed to PrepareProfile method call.
  virtual void DelegateDeleted(Delegate* delegate) = 0;

  // Invoked after the tmpfs is successfully mounted.
  // Asks session manager to restart Chrome in Browse Without Sign In mode.
  // |start_url| is url for launched browser to open.
  virtual void CompleteOffTheRecordLogin(const GURL& start_url) = 0;

  // Invoked when the user is logging in for the first time, or is logging in as
  // a guest user.
  virtual void SetFirstLoginPrefs(PrefService* prefs) = 0;

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
      LoginStatusConsumer* consumer) = 0;

  // Prewarms the authentication network connection.
  virtual void PrewarmAuthentication() = 0;

  // Restores authentication session after crash.
  virtual void RestoreAuthenticationSession(Profile* profile) = 0;

  // Stops background fetchers.
  virtual void StopBackgroundFetchers() = 0;

  // Initialize RLZ.
  virtual void InitRlzDelayed(Profile* user_profile) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
