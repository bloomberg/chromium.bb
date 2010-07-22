// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lock.h"
#include "base/nss_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/time.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/external_cookie_handler.h"
#include "chrome/browser/chromeos/login/cookie_fetcher.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/user_image_downloader.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "views/widget/widget_gtk.h"

namespace chromeos {

namespace {

static char kIncognitoUser[] = "incognito";

// Prefix for Auth token received from ClientLogin request.
const char kAuthPrefix[] = "Auth=";
// Suffix for Auth token received from ClientLogin request.
const char kAuthSuffix[] = "\n";

}  // namespace

class LoginUtilsImpl : public LoginUtils,
                       public NotificationObserver {
 public:
  LoginUtilsImpl()
      : browser_launch_enabled_(true) {
    registrar_.Add(
        this,
        NotificationType::LOGIN_USER_CHANGED,
        NotificationService::AllSources());
  }

  // Invoked after the user has successfully logged in. This launches a browser
  // and does other bookkeeping after logging in.
  virtual void CompleteLogin(const std::string& username,
      const GaiaAuthConsumer::ClientLoginResult& credentials);

  // Invoked after the tmpfs is successfully mounted.
  // Launches a browser in the off the record (incognito) mode.
  virtual void CompleteOffTheRecordLogin();

  // Creates and returns the authenticator to use. The caller owns the returned
  // Authenticator and must delete it when done.
  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer);

  // Used to postpone browser launch via DoBrowserLaunch() if some post
  // login screen is to be shown.
  virtual void EnableBrowserLaunch(bool enable);

  // Returns if browser launch enabled now or not.
  virtual bool IsBrowserLaunchEnabled() const;

  // Returns auth token for 'cp' Contacts service.
  virtual const std::string& GetAuthToken() const { return auth_token_; }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Attempt to connect to the preferred network if available.
  void ConnectToPreferredNetwork();

  NotificationRegistrar registrar_;

  // Indicates if DoBrowserLaunch will actually launch the browser or not.
  bool browser_launch_enabled_;

  // Auth token for Contacts service. Received by GoogleAuthenticator as
  // part of ClientLogin response.
  std::string auth_token_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsImpl);
};

class LoginUtilsWrapper {
 public:
  LoginUtilsWrapper() {}

  LoginUtils* get() {
    AutoLock create(create_lock_);
    if (!ptr_.get())
      reset(new LoginUtilsImpl);
    return ptr_.get();
  }

  void reset(LoginUtils* ptr) {
    ptr_.reset(ptr);
  }

 private:
  Lock create_lock_;
  scoped_ptr<LoginUtils> ptr_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsWrapper);
};

void LoginUtilsImpl::CompleteLogin(const std::string& username,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {

  LOG(INFO) << "Completing login for " << username;

  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetLoginLibrary()->StartSession(username, "");

  UserManager::Get()->UserLoggedIn(username);
  ConnectToPreferredNetwork();

  // Now launch the initial browser window.
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The default profile will have been changed because the ProfileManager
  // will process the notification that the UserManager sends out.
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);

  logging::RedirectChromeLogging(
      user_data_dir.Append(profile_manager->GetCurrentProfileDir()),
      *(CommandLine::ForCurrentProcess()),
      logging::DELETE_OLD_LOG_FILE);

  // Supply credentials for sync and others to use
  profile->GetTokenService()->SetClientLoginResult(credentials);

  // Take the credentials passed in and try to exchange them for
  // full-fledged Google authentication cookies.  This is
  // best-effort; it's possible that we'll fail due to network
  // troubles or some such.  Either way, |cf| will call
  // DoBrowserLaunch on the UI thread when it's done, and then
  // delete itself.
  CookieFetcher* cf = new CookieFetcher(profile);
  cf->AttemptFetch(credentials.data);
  auth_token_ = credentials.token;
}

void LoginUtilsImpl::CompleteOffTheRecordLogin() {
  LOG(INFO) << "Completing off the record login";

  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetLoginLibrary()->StartSession(kIncognitoUser, "");

  // Incognito flag is not set by default.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kIncognito);

  UserManager::Get()->OffTheRecordUserLoggedIn();
  ConnectToPreferredNetwork();
  LoginUtils::DoBrowserLaunch(
      ProfileManager::GetDefaultProfile()->GetOffTheRecordProfile());
}

Authenticator* LoginUtilsImpl::CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  return new GoogleAuthenticator(consumer);
}

void LoginUtilsImpl::EnableBrowserLaunch(bool enable) {
  browser_launch_enabled_ = enable;
}

bool LoginUtilsImpl::IsBrowserLaunchEnabled() const {
  return browser_launch_enabled_;
}

void LoginUtilsImpl::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::LOGIN_USER_CHANGED)
    base::OpenPersistentNSSDB();
}

void LoginUtilsImpl::ConnectToPreferredNetwork() {
  CrosLibrary::Get()->GetNetworkLibrary()->
      ConnectToPreferredNetworkIfAvailable();
}

LoginUtils* LoginUtils::Get() {
  return Singleton<LoginUtilsWrapper>::get()->get();
}

void LoginUtils::Set(LoginUtils* mock) {
  Singleton<LoginUtilsWrapper>::get()->reset(mock);
}

void LoginUtils::DoBrowserLaunch(Profile* profile) {
  // Browser launch was disabled due to some post login screen.
  if (!LoginUtils::Get()->IsBrowserLaunchEnabled())
    return;

  // Update command line in case loose values were added.
  CommandLine::ForCurrentProcess()->InitFromArgv(
      CommandLine::ForCurrentProcess()->argv());

  LOG(INFO) << "Launching browser...";
  BrowserInit browser_init;
  int return_code;
  browser_init.LaunchBrowser(*CommandLine::ForCurrentProcess(),
                             profile,
                             std::wstring(),
                             true,
                             &return_code);
}

std::string LoginUtils::ExtractClientLoginParam(
    const std::string& credentials,
    const std::string& param_prefix,
    const std::string& param_suffix) {
  size_t start = credentials.find(param_prefix);
  if (start == std::string::npos)
    return std::string();
  start += param_prefix.size();
  size_t end = credentials.find(param_suffix, start);
  if (end == std::string::npos)
    return std::string();
  return credentials.substr(start, end - start);
}

}  // namespace chromeos
