// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
#pragma once

#include <string>

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
class BackgroundView;
class LoginStatusConsumer;

class LoginUtils {
 public:
  // Get LoginUtils singleton object. If it was not set before, new default
  // instance will be created.
  static LoginUtils* Get();

  // Set LoginUtils singleton object for test purpose only!
  static void Set(LoginUtils* ptr);

  // Thin wrapper around BrowserInit::LaunchBrowser().  Meant to be used in a
  // Task posted to the UI thread.
  static void DoBrowserLaunch(Profile* profile);

  virtual ~LoginUtils() {}

  // Invoked after the user has successfully logged in. This launches a browser
  // and does other bookkeeping after logging in.
  // If |pending_requests| is true, there's a pending online auth request.
  virtual void CompleteLogin(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests) = 0;

  // Invoked after the tmpfs is successfully mounted.
  // Asks session manager to restart Chrome in Browse Without Sign In mode.
  // |start_url| is url for launched browser to open.
  virtual void CompleteOffTheRecordLogin(const GURL& start_url) = 0;

  // Invoked when the user is logging in for the first time, or is logging in as
  // a guest user.
  virtual void SetFirstLoginPrefs(PrefService* prefs) = 0;

  // Creates and returns the authenticator to use. The caller owns the returned
  // Authenticator and must delete it when done.
  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer) = 0;

  // Used to postpone browser launch via DoBrowserLaunch() if some post
  // login screen is to be shown.
  virtual void EnableBrowserLaunch(bool enable) = 0;

  // Returns if browser launch enabled now or not.
  virtual bool IsBrowserLaunchEnabled() const = 0;

  // Prewarms the authentication network connection.
  virtual void PrewarmAuthentication() = 0;

  // Given the credentials try to exchange them for
  // full-fledged Google authentication cookies.
  virtual void FetchCookies(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) = 0;

  // Supply credentials for sync and others to use.
  virtual void FetchTokens(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) = 0;

  // Sets the current background view.
  virtual void SetBackgroundView(BackgroundView* background_view) = 0;

  // Gets the current background view.
  virtual BackgroundView* GetBackgroundView() = 0;

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
