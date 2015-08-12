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
class CookieOSCryptoDelegate : public net::CookieCryptoDelegate {
 public:
  bool ShouldEncrypt() override;
  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;
  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;
};

bool CookieOSCryptoDelegate::ShouldEncrypt() {
#if defined(OS_IOS)
  // Cookie encryption is not necessary on iOS, due to OS-protected storage.
  // However, due to https://codereview.chromium.org/135183021/, cookies were
  // accidentally encrypted. In order to allow these cookies to still be used,a
  // a CookieCryptoDelegate is provided that can decrypt existing cookies.
  // However, new cookies will not be encrypted. The alternatives considered
  // were not supplying a delegate at all (thus invalidating all existing
  // encrypted cookies) or in migrating all cookies at once, which may impose
  // startup costs.  Eventually, all cookies will get migrated as they are
  // rewritten.
  return false;
#else
  return true;
#endif
}

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
