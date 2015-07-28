// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/cookie_store_util.h"

#include "base/lazy_instance.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/evicted_domain_cookie_counter.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "components/os_crypt/os_crypt.h"
#include "content/public/common/content_constants.h"
#include "extensions/common/constants.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"

namespace chrome_browser_net {

net::CookieMonsterDelegate* CreateCookieDelegate(
    scoped_refptr<net::CookieMonsterDelegate> next_cookie_monster_delegate) {
  return new EvictedDomainCookieCounter(next_cookie_monster_delegate);
}

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
namespace {

// Use the operating system's mechanisms to encrypt cookies before writing
// them to persistent store.  Currently this only is done with desktop OS's
// because ChromeOS and Android already protect the entire profile contents.
//
// TODO(bcwhite): Enable on MACOSX -- requires all Cookie tests to call
// OSCrypt::UseMockKeychain or will hang waiting for user input.
class CookieOSCryptoDelegate : public net::CookieCryptoDelegate {
 public:
  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;
  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;
};

bool CookieOSCryptoDelegate::EncryptString(const std::string& plaintext,
                                           std::string* ciphertext) {
  return OSCrypt::EncryptString(plaintext, ciphertext);
}

bool CookieOSCryptoDelegate::DecryptString(const std::string& ciphertext,
                                           std::string* plaintext) {
  return OSCrypt::DecryptString(ciphertext, plaintext);
}

// Using a LazyInstance is safe here because this class is stateless and
// requires 0 initialization.
base::LazyInstance<CookieOSCryptoDelegate> g_cookie_crypto_delegate =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

net::CookieCryptoDelegate* GetCookieCryptoDelegate() {
  return g_cookie_crypto_delegate.Pointer();
}
#else  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
net::CookieCryptoDelegate* GetCookieCryptoDelegate() {
  return NULL;
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

}  // namespace chrome_browser_net
