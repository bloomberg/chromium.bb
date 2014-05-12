// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/cookie_store_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/evicted_domain_cookie_counter.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "components/os_crypt/os_crypt.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_crypto_delegate.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_constants.h"
#include "extensions/common/constants.h"

using content::BrowserThread;

namespace {

class ChromeCookieMonsterDelegate : public net::CookieMonsterDelegate {
 public:
  explicit ChromeCookieMonsterDelegate(Profile* profile)
      : profile_getter_(
          base::Bind(&GetProfileOnUI, g_browser_process->profile_manager(),
                     profile)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(profile);
  }

  // net::CookieMonster::Delegate implementation.
  virtual void OnCookieChanged(
      const net::CanonicalCookie& cookie,
      bool removed,
      net::CookieMonster::Delegate::ChangeCause cause) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ChromeCookieMonsterDelegate::OnCookieChangedAsyncHelper,
                   this, cookie, removed, cause));
  }

  virtual void OnLoaded() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ChromeCookieMonsterDelegate::OnLoadedAsyncHelper,
                   this));
  }

 private:
  virtual ~ChromeCookieMonsterDelegate() {}

  static Profile* GetProfileOnUI(ProfileManager* profile_manager,
                                 Profile* profile) {
    if (profile_manager->IsValidProfile(profile))
      return profile;
    return NULL;
  }

  void OnCookieChangedAsyncHelper(
      const net::CanonicalCookie& cookie,
      bool removed,
      net::CookieMonster::Delegate::ChangeCause cause) {
    Profile* profile = profile_getter_.Run();
    if (profile) {
      ChromeCookieDetails cookie_details(&cookie, removed, cause);
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_COOKIE_CHANGED,
          content::Source<Profile>(profile),
          content::Details<ChromeCookieDetails>(&cookie_details));
    }
  }

  void OnLoadedAsyncHelper() {
    Profile* profile = profile_getter_.Run();
    if (profile) {
      prerender::PrerenderManager* prerender_manager =
          prerender::PrerenderManagerFactory::GetForProfile(profile);
      if (prerender_manager)
        prerender_manager->OnCookieStoreLoaded();
    }
  }

  const base::Callback<Profile*(void)> profile_getter_;
};

}  // namespace

namespace chrome_browser_net {

bool IsCookieRecordMode() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  // Only allow Record Mode if we are in a Debug build or where we are running
  // a cycle, and the user has limited control.
  return command_line.HasSwitch(switches::kRecordMode) &&
      chrome::kRecordModeEnabled;
}

bool ShouldUseInMemoryCookiesAndCache() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  return IsCookieRecordMode() ||
      command_line.HasSwitch(switches::kPlaybackMode);
}

net::CookieMonsterDelegate* CreateCookieDelegate(Profile* profile) {
  return new EvictedDomainCookieCounter(
      new ChromeCookieMonsterDelegate(profile));
}

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
namespace {

// Use the operating system's mechanisms to encrypt cookies before writing
// them to persistent store.  Currently this only is done with desktop OS's
// because ChromeOS and Android already protect the entire profile contents.
//
// TODO(bcwhite): Enable on MACOSX -- requires all Cookie tests to call
// OSCrypt::UseMockKeychain or will hang waiting for user input.
class CookieOSCryptoDelegate : public content::CookieCryptoDelegate {
 public:
  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) OVERRIDE;
  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) OVERRIDE;
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

content::CookieCryptoDelegate* GetCookieCryptoDelegate() {
  return g_cookie_crypto_delegate.Pointer();
}
#else  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
content::CookieCryptoDelegate* GetCookieCryptoDelegate() {
  return NULL;
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

}  // namespace chrome_browser_net
