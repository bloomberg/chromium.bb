// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/lock.h"
#include "base/nss_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/time.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/external_cookie_handler.h"
#include "chrome/browser/chromeos/login/cookie_fetcher.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/user_image_downloader.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
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

const int kWifiTimeoutInMS = 15000;

// Prefix for Auth token received from ClientLogin request.
const char kAuthPrefix[] = "Auth=";
// Suffix for Auth token received from ClientLogin request.
const char kAuthSuffix[] = "\n";

// Find Auth token in given response from ClientLogin request.
// Returns the token if found, empty string otherwise.
std::string get_auth_token(const std::string& credentials) {
  size_t auth_start = credentials.find(kAuthPrefix);
  if (auth_start == std::string::npos)
    return std::string();
  auth_start += arraysize(kAuthPrefix) - 1;
  size_t auth_end = credentials.find(kAuthSuffix, auth_start);
  if (auth_end == std::string::npos)
    return std::string();
  return credentials.substr(auth_start, auth_end - auth_start);
}



class LoginUtilsImpl : public LoginUtils,
                       public NotificationObserver {
 public:
  LoginUtilsImpl() : wifi_connecting_(false) {
    registrar_.Add(
        this,
        NotificationType::LOGIN_USER_CHANGED,
        NotificationService::AllSources());
  }

  virtual bool ShouldWaitForWifi();

  // Invoked after the user has successfully logged in. This launches a browser
  // and does other bookkeeping after logging in.
  virtual void CompleteLogin(const std::string& username,
                             const std::string& credentials);

  // Creates and returns the authenticator to use. The caller owns the returned
  // Authenticator and must delete it when done.
  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  NotificationRegistrar registrar_;
  bool wifi_connecting_;
  base::Time wifi_connect_start_time_;

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

bool LoginUtilsImpl::ShouldWaitForWifi() {
  // If we are connecting to the preferred network,
  // wait kWifiTimeoutInMS until connection is made or failed.
  if (wifi_connecting_) {
    NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
    if (cros->PreferredNetworkConnected()) {
      LOG(INFO) << "Wifi connection successful.";
      return false;
    } else if (cros->PreferredNetworkFailed()) {
      LOG(INFO) << "Wifi connection failed.";
      return false;
    } else {
      base::TimeDelta diff = base::Time::Now() - wifi_connect_start_time_;
      // Keep waiting if we have not timed out yet.
      bool timedout = (diff.InMilliseconds() > kWifiTimeoutInMS);
      if (timedout)
        LOG(INFO) << "Wifi connection timed out.";
      return !timedout;
    }
  }
  return false;
}

void LoginUtilsImpl::CompleteLogin(const std::string& username,
                                   const std::string& credentials) {
  LOG(INFO) << "Completing login for " << username;

  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetLoginLibrary()->StartSession(username, "");

  UserManager::Get()->UserLoggedIn(username);

  // This will attempt to connect to the preferred network if available.
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  wifi_connecting_ = cros->ConnectToPreferredNetworkIfAvailable();
  if (wifi_connecting_)
    wifi_connect_start_time_ = base::Time::Now();

  // Now launch the initial browser window.
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The default profile will have been changed because the ProfileManager
  // will process the notification that the UserManager sends out.
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);

  // Take the credentials passed in and try to exchange them for
  // full-fledged Google authentication cookies.  This is
  // best-effort; it's possible that we'll fail due to network
  // troubles or some such.  Either way, |cf| will call
  // DoBrowserLaunch on the UI thread when it's done, and then
  // delete itself.
  CookieFetcher* cf = new CookieFetcher(profile);
  cf->AttemptFetch(credentials);
  std::string auth = get_auth_token(credentials);
  if (!auth.empty())
    new UserImageDownloader(username, auth);
}

Authenticator* LoginUtilsImpl::CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  return new GoogleAuthenticator(consumer);
}

void LoginUtilsImpl::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::LOGIN_USER_CHANGED)
    base::OpenPersistentNSSDB();
}

}  // namespace

LoginUtils* LoginUtils::Get() {
  return Singleton<LoginUtilsWrapper>::get()->get();
}

void LoginUtils::Set(LoginUtils* mock) {
  Singleton<LoginUtilsWrapper>::get()->reset(mock);
}

void LoginUtils::DoBrowserLaunch(Profile* profile) {
  // If we should wait for wifi connection, post this task with a 100ms delay.
  if (LoginUtils::Get()->ShouldWaitForWifi()) {
    ChromeThread::PostDelayedTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(&LoginUtils::DoBrowserLaunch, profile), 100);
    return;
  }

  LOG(INFO) << "Launching browser...";
  BrowserInit browser_init;
  int return_code;
  browser_init.LaunchBrowser(*CommandLine::ForCurrentProcess(),
                             profile,
                             std::wstring(),
                             true,
                             &return_code);
}

}  // namespace chromeos
