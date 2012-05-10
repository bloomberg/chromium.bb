// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"

class CommandLine;
class GURL;
class Profile;
class PrefService;

namespace {
class BrowserGuestSessionNavigatorTest;
}  // namespace

namespace chromeos {

class Authenticator;
class LoginDisplayHost;
class LoginStatusConsumer;

class LoginUtils {
 public:
  class Delegate {
   public:
    // Called after profile is loaded and prepared for the session.
    virtual void OnProfilePrepared(Profile* profile) = 0;
  };

  // Get LoginUtils singleton object. If it was not set before, new default
  // instance will be created.
  static LoginUtils* Get();

  // Set LoginUtils singleton object for test purpose only!
  static void Set(LoginUtils* ptr);

  // Checks if the given username is whitelisted and allowed to sign-in to
  // this device.
  static bool IsWhitelisted(const std::string& username);

  virtual ~LoginUtils() {}

  // Thin wrapper around BrowserInit::LaunchBrowser().  Meant to be used in a
  // Task posted to the UI thread.  Once the browser is launched the login
  // host is deleted.
  virtual void DoBrowserLaunch(Profile* profile,
                               LoginDisplayHost* login_host) = 0;

  // Loads and prepares profile for the session. Fires |delegate| in the end.
  // If |pending_requests| is true, there's a pending online auth request.
  // If |display_email| is not empty, user's displayed email will be set to
  // this value, shown in UI.
  // Also see DelegateDeleted method.
  virtual void PrepareProfile(
      const std::string& username,
      const std::string& display_email,
      const std::string& password,
      bool pending_requests,
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

  // Starts process of fetching OAuth2 tokens (based on OAuth1 tokens found
  // in |user_profile|) and kicks off internal services that depend on them.
  virtual void StartTokenServices(Profile* user_profile) = 0;

  // Supply credentials for sync and others to use.
  virtual void StartSignedInServices(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) = 0;

  // Transfers cookies from the |default_profile| into the |new_profile|.
  // If authentication was performed by an extension, then
  // the set of cookies that was acquired through such that process will be
  // automatically transfered into the profile.
  virtual void TransferDefaultCookies(Profile* default_profile,
                                      Profile* new_profile) = 0;

  // Transfers HTTP authentication cache from the |default_profile|
  // into the |new_profile|. If user was required to authenticate with a proxy
  // during the login, this authentication information will be transferred
  // into the new session.
  virtual void TransferDefaultAuthCache(Profile* default_profile,
                                        Profile* new_profile) = 0;

  // Stops background fetchers.
  virtual void StopBackgroundFetchers() = 0;

 protected:
  friend class ::BrowserGuestSessionNavigatorTest;

  // Returns command line string to be used for the OTR process. Also modifies
  // given command line.
  virtual std::string GetOffTheRecordCommandLine(
      const GURL& start_url,
      const CommandLine& base_command_line,
      CommandLine* command_line) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
