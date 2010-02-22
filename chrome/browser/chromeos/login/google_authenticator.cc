// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/load_flags.h"
#include "third_party/libjingle/files/talk/base/urlencode.h"

const char GoogleAuthenticator::kClientLoginUrl[] =
    "https://www.google.com/accounts/ClientLogin";
const char GoogleAuthenticator::kFormat[] =
    "Email=%s&"
    "Passwd=%s&"
    "PersistentCookie=%s&"
    "accountType=%s&"
    "source=%s&";
const char GoogleAuthenticator::kCookiePersistence[] = "true";
const char GoogleAuthenticator::kAccountType[] = "HOSTED_OR_GOOGLE";
const char GoogleAuthenticator::kSource[] = "chromeos";

bool GoogleAuthenticator::Authenticate(const std::string& username,
                                       const std::string& password) {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  URLRequestContextGetter *getter =
      profile->GetOffTheRecordProfile()->GetRequestContext();
  fetcher_.reset(new URLFetcher(GURL(kClientLoginUrl),
                                URLFetcher::POST,
                                this));
  fetcher_->set_request_context(getter);
  fetcher_->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES);
  // TODO(cmasone): be more careful about zeroing memory that stores
  // the user's password.
  std::string body = StringPrintf(kFormat,
                                  UrlEncodeString(username).c_str(),
                                  UrlEncodeString(password).c_str(),
                                  kCookiePersistence,
                                  kAccountType,
                                  kSource);
  fetcher_->set_upload_data("application/x-www-form-urlencoded", body);
  fetcher_->Start();
  username_.assign(username);
  return true;
}

void GoogleAuthenticator::OnURLFetchComplete(const URLFetcher* source,
                                             const GURL& url,
                                             const URLRequestStatus& status,
                                             int response_code,
                                             const ResponseCookies& cookies,
                                             const std::string& data) {
  // TODO(cmasone):
  // If successful, post a task to the UI thread with the cookies.
  // That task will mount cryptohome (has to be on the UI thread, as
  // it talks to dbus...eww) and then tell the UserManager that the
  // user logged in, get the new profile, and then inject cookies.
  // Then, do all the BrowserInit stuff.

  LOG(INFO) << "ClientLogin response code: " << response_code;
  if (200 == response_code) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(
            GoogleAuthenticator::OnLoginSuccess, consumer_, username_));
  } else {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(GoogleAuthenticator::OnLoginFailure, consumer_));
  }
}

// static
void GoogleAuthenticator::OnLoginSuccess(LoginStatusConsumer* consumer,
                                         const std::string& username) {
  consumer->OnLoginSuccess(username);
}

// static
void GoogleAuthenticator::OnLoginFailure(LoginStatusConsumer* consumer) {
  consumer->OnLoginFailure();
}
