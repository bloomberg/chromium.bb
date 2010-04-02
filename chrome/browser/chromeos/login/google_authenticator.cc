// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <string>
#include <vector>
#include <time.h>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "base/third_party/nss/blapi.h"
#include "base/third_party/nss/sha256.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/client_login_response_handler.h"
#include "chrome/browser/chromeos/login/issue_response_handler.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"
#include "third_party/libjingle/files/talk/base/urlencode.h"

using base::Time;
using base::TimeDelta;
using namespace chromeos;
using namespace file_util;

const char GoogleAuthenticator::kFormat[] =
    "Email=%s&"
    "Passwd=%s&"
    "PersistentCookie=%s&"
    "accountType=%s&"
    "source=%s&";
const char GoogleAuthenticator::kCookiePersistence[] = "true";
const char GoogleAuthenticator::kAccountType[] = "HOSTED_OR_GOOGLE";
const char GoogleAuthenticator::kSource[] = "chromeos";
const int GoogleAuthenticator::kHttpSuccess = 200;
const int kPassHashLen = 32;

// Chromium OS system salt stored here
const char GoogleAuthenticator::kSystemSalt[] = "/home/.shadow/salt";

// String that appears at the start of OpenSSL cipher text with embedded salt
const char GoogleAuthenticator::kOpenSSLMagic[] = "Salted__";

bool GoogleAuthenticator::Authenticate(const std::string& username,
                                       const std::string& password) {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  getter_ = profile->GetRequestContext();
  fetcher_.reset(new URLFetcher(GURL(AuthResponseHandler::kClientLoginUrl),
                                URLFetcher::POST,
                                this));
  fetcher_->set_request_context(getter_);
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
  if (!client_login_handler_.get())
    client_login_handler_.reset(new ClientLoginResponseHandler(getter_));
  if (!issue_handler_.get())
    issue_handler_.reset(new IssueResponseHandler(getter_));
  username_.assign(username);
  StoreHashedPassword(password);
  return true;
}

void GoogleAuthenticator::OnURLFetchComplete(const URLFetcher* source,
                                             const GURL& url,
                                             const URLRequestStatus& status,
                                             int response_code,
                                             const ResponseCookies& cookies,
                                             const std::string& data) {
  if (status.is_success() && response_code == 200) {
    if (client_login_handler_->CanHandle(url)) {
      fetcher_.reset(client_login_handler_->Handle(data, this));
    } else if (issue_handler_->CanHandle(url)) {
      fetcher_.reset(issue_handler_->Handle(data, this));
    } else {
      LOG(INFO) << "Online login successful!";
      ChromeThread::PostTask(
          ChromeThread::UI, FROM_HERE,
          NewRunnableFunction(GoogleAuthenticator::OnLoginSuccess,
                              consumer_,
                              username_,
                              ascii_hash_,
                              cookies));
    }
  } else if (!status.is_success()) {
    LOG(INFO) << "Network fail";
    // The fetch failed for network reasons, try offline login.
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(GoogleAuthenticator::CheckOffline,
                            consumer_,
                            username_,
                            ascii_hash_,
                            status));
  } else {
    std::string error;
    if (status.is_success()) {
      // The fetch succeeded, but ClientLogin said no.
      error.assign(data);
    } else {
      // We couldn't hit the network, and offline login failed.
      error.assign(strerror(status.os_error()));
    }
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(
            GoogleAuthenticator::OnLoginFailure, consumer_, error));
  }
}

// static
void GoogleAuthenticator::OnLoginSuccess(LoginStatusConsumer* consumer,
                                         const std::string& username,
                                         const std::string& passhash,
                                         const ResponseCookies& cookies) {
  if (CrosLibrary::Get()->GetCryptohomeLibrary()->Mount(username.c_str(),
                                                        passhash.c_str()))
    consumer->OnLoginSuccess(username, cookies);
  else
    GoogleAuthenticator::OnLoginFailure(consumer, "Could not mount cryptohome");
}

// static
void GoogleAuthenticator::CheckOffline(LoginStatusConsumer* consumer,
                                       const std::string& username,
                                       const std::string& passhash,
                                       const URLRequestStatus& status) {
  if (CrosLibrary::Get()->GetCryptohomeLibrary()->CheckKey(username.c_str(),
                                                           passhash.c_str())) {
    // The fetch didn't succeed, but offline login did.
    LOG(INFO) << "Offline login successful!";
    ResponseCookies cookies;
    GoogleAuthenticator::OnLoginSuccess(consumer,
                                        username,
                                        passhash,
                                        cookies);
  } else {
    // We couldn't hit the network, and offline login failed.
    GoogleAuthenticator::OnLoginFailure(consumer, strerror(status.os_error()));
  }
}

// static
void GoogleAuthenticator::OnLoginFailure(LoginStatusConsumer* consumer,
                                         const std::string& data) {
  // TODO(cmasone): what can we do to expose these OS/server-side error strings
  // in an internationalizable way?
  consumer->OnLoginFailure(data);
}

void GoogleAuthenticator::LoadSystemSalt(FilePath& path) {
  if (!system_salt_.empty())
    return;

  CHECK(PathExists(path)) << path.value() << " does not exist!";

  int64 file_size;
  CHECK(GetFileSize(path, &file_size)) << "Could not get size of "
                                       << path.value();

  char salt[file_size];
  int data_read = ReadFile(path, salt, file_size);

  CHECK_EQ(data_read % 2, 0);
  system_salt_.assign(salt, salt + data_read);
}

std::string GoogleAuthenticator::SaltAsAscii() {
  FilePath salt_path(kSystemSalt);
  LoadSystemSalt(salt_path);
  unsigned int salt_len = system_salt_.size();
  char ascii_salt[2 * salt_len + 1];
  if (GoogleAuthenticator::BinaryToHex(system_salt_,
                                       salt_len,
                                       ascii_salt,
                                       sizeof(ascii_salt))) {
    return std::string(ascii_salt, sizeof(ascii_salt) - 1);
  } else {
    return std::string();
  }
}

void GoogleAuthenticator::StoreHashedPassword(const std::string& password) {
  // Get salt, ascii encode, update sha with that, then update with ascii
  // of password, then end.
  std::string ascii_salt = SaltAsAscii();
  unsigned char passhash_buf[kPassHashLen];
  char ascii_buf[kPassHashLen + 1];

  // Hash salt and password
  SHA256Context ctx;
  SHA256_Begin(&ctx);
  SHA256_Update(&ctx,
                reinterpret_cast<const unsigned char*>(ascii_salt.data()),
                static_cast<unsigned int>(ascii_salt.length()));
  SHA256_Update(&ctx,
                reinterpret_cast<const unsigned char*>(password.data()),
                static_cast<unsigned int>(password.length()));
  SHA256_End(&ctx,
             passhash_buf,
             NULL,
             static_cast<unsigned int>(sizeof(passhash_buf)));

  std::vector<unsigned char> passhash(passhash_buf,
                                      passhash_buf + sizeof(passhash_buf));
  BinaryToHex(passhash,
              passhash.size() / 2,  // only want top half, at least for now.
              ascii_buf,
              sizeof(ascii_buf));
  ascii_hash_.assign(ascii_buf, sizeof(ascii_buf) - 1);
}

// static
bool GoogleAuthenticator::BinaryToHex(const std::vector<unsigned char>& binary,
                                      const unsigned int binary_len,
                                      char* hex_string,
                                      const unsigned int len) {
  if (len < 2*binary_len)
    return false;
  memset(hex_string, 0, len);
  for (uint i = 0, j = 0; i < binary_len; i++, j+=2)
    snprintf(hex_string + j, len - j, "%02x", binary[i]);
  return true;
}
