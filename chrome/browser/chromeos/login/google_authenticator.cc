// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/google_authenticator.h"

#include <errno.h>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "base/third_party/nss/blapi.h"
#include "base/third_party/nss/sha256.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/auth_response_handler.h"
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
using namespace file_util;

namespace chromeos {

// static
const char GoogleAuthenticator::kCookiePersistence[] = "true";
// static
const char GoogleAuthenticator::kAccountType[] = "HOSTED_OR_GOOGLE";
// static
const char GoogleAuthenticator::kSource[] = "chromeos";
// static
const char GoogleAuthenticator::kFormat[] =
    "Email=%s&"
    "Passwd=%s&"
    "PersistentCookie=%s&"
    "accountType=%s&"
    "source=%s&";

// static
const char GoogleAuthenticator::kSystemSalt[] = "/home/.shadow/salt";
// static
const char GoogleAuthenticator::kOpenSSLMagic[] = "Salted__";
// static
const char GoogleAuthenticator::kLocalaccountFile[] = "localaccount";
// static
const char GoogleAuthenticator::kTmpfsTrigger[] = "incognito";

const int kPassHashLen = 32;

GoogleAuthenticator::GoogleAuthenticator(LoginStatusConsumer* consumer)
    : Authenticator(consumer),
      fetcher_(NULL),
      getter_(NULL),
      checked_for_localaccount_(false) {
  CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
}

GoogleAuthenticator::~GoogleAuthenticator() {
  ChromeThread::DeleteSoon(ChromeThread::FILE, FROM_HERE, fetcher_);
}

bool GoogleAuthenticator::Authenticate(Profile* profile,
                                       const std::string& username,
                                       const std::string& password) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  getter_ = profile->GetRequestContext();
  fetcher_ = URLFetcher::Create(0,
                                GURL(AuthResponseHandler::kClientLoginUrl),
                                URLFetcher::POST,
                                this);
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
  username_.assign(username);
  StoreHashedPassword(password);
  fetcher_->Start();
  return true;
}

void GoogleAuthenticator::OnURLFetchComplete(const URLFetcher* source,
                                             const GURL& url,
                                             const URLRequestStatus& status,
                                             int response_code,
                                             const ResponseCookies& cookies,
                                             const std::string& data) {
  if (status.is_success() && response_code == kHttpSuccess) {
    LOG(INFO) << "Online login successful!";
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &GoogleAuthenticator::OnLoginSuccess, data));
  } else if (!status.is_success()) {
    LOG(INFO) << "Network fail";
    // The fetch failed for network reasons, try offline login.
    LoadLocalaccount(kLocalaccountFile);
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &GoogleAuthenticator::CheckOffline, status));
  } else {
    // The fetch succeeded, but ClientLogin said no.
    LoadLocalaccount(kLocalaccountFile);
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &GoogleAuthenticator::CheckLocalaccount, data));
  }
}

void GoogleAuthenticator::OnLoginSuccess(const std::string& data) {
  if (CrosLibrary::Get()->GetCryptohomeLibrary()->Mount(username_.c_str(),
                                                        ascii_hash_.c_str())) {
    consumer_->OnLoginSuccess(username_, data);
  } else {
    OnLoginFailure("Could not mount cryptohome");
  }
}

void GoogleAuthenticator::CheckOffline(const URLRequestStatus& status) {
  if (CrosLibrary::Get()->GetCryptohomeLibrary()->CheckKey(
          username_.c_str(),
          ascii_hash_.c_str())) {
    // The fetch didn't succeed, but offline login did.
    LOG(INFO) << "Offline login successful!";
    OnLoginSuccess(std::string());
  } else {
    // We couldn't hit the network, and offline login failed.
    GoogleAuthenticator::CheckLocalaccount(strerror(status.os_error()));
  }
}

void GoogleAuthenticator::CheckLocalaccount(const std::string& error) {
  if (!localaccount_.empty() && localaccount_ == username_ &&
      CrosLibrary::Get()->GetCryptohomeLibrary()->Mount(kTmpfsTrigger, "")) {
    LOG(WARNING) << "Logging in with localaccount: " << localaccount_;
    consumer_->OnLoginSuccess(username_, std::string());
  } else {
    OnLoginFailure(error);
  }
}

void GoogleAuthenticator::OnLoginFailure(const std::string& data) {
  LOG(WARNING) << "Login failed: " << data;
  // TODO(cmasone): what can we do to expose these OS/server-side error strings
  // in an internationalizable way?
  consumer_->OnLoginFailure(data);
}

void GoogleAuthenticator::LoadSystemSalt(const FilePath& path) {
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

void GoogleAuthenticator::LoadLocalaccount(const std::string& filename) {
  if (checked_for_localaccount_)
    return;
  FilePath localaccount_file;
  std::string localaccount;
  if (PathService::Get(base::DIR_EXE, &localaccount_file)) {
    localaccount_file = localaccount_file.Append(filename);
    LOG(INFO) << "looking for localaccount in " << localaccount_file.value();

    ReadFileToString(localaccount_file, &localaccount);
    TrimWhitespaceASCII(localaccount, TRIM_TRAILING, &localaccount);
    LOG(INFO) << "Loading localaccount: " << localaccount;
  } else {
    LOG(INFO) << "Assuming no localaccount";
  }
  set_localaccount(localaccount);
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

std::string GoogleAuthenticator::SaltAsAscii() {
  LoadSystemSalt(FilePath(kSystemSalt));  // no-op if it's already loaded.
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

}  // namespace chromeos
