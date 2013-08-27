// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/cookie_store_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/evicted_domain_cookie_counter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
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

  const base::Callback<Profile*(void)> profile_getter_;
};

}  // namespace

namespace chrome_browser_net {

bool IsCookieRecordMode() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  // Only allow Record Mode if we are in a Debug build or where we are running
  // a cycle, and the user has limited control.
  return command_line.HasSwitch(switches::kRecordMode) &&
      (chrome::kRecordModeEnabled ||
       command_line.HasSwitch(switches::kVisitURLs));
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

void SetCookieStoreConfigs(
    const base::FilePath& partition_path,
    bool in_memory_partition,
    bool is_default_partition,
    content::CookieStoreConfig::SessionCookieMode session_cookie_mode,
    quota::SpecialStoragePolicy* storage_policy,
    net::CookieMonsterDelegate* cookie_delegate,
    content::BrowserContext::CookieSchemeMap* configs) {
  using content::CookieStoreConfig;
  configs->clear();

  bool in_memory = in_memory_partition ||
      chrome_browser_net::ShouldUseInMemoryCookiesAndCache();

  if (in_memory) {
    (*configs)[content::BrowserContext::kDefaultCookieScheme] =
        CookieStoreConfig(base::FilePath(),
                          CookieStoreConfig::EPHEMERAL_SESSION_COOKIES,
                          storage_policy,
                          cookie_delegate);
  } else {
    (*configs)[content::BrowserContext::kDefaultCookieScheme] =
        CookieStoreConfig(partition_path.Append(content::kCookieFilename),
                          session_cookie_mode,
                          storage_policy,
                          cookie_delegate);
  }

  // Handle adding the extensions cookie store.
  if (is_default_partition) {
    if (in_memory) {
      (*configs)[extensions::kExtensionScheme] = CookieStoreConfig();
    } else {
      base::FilePath cookie_path = partition_path.Append(
          chrome::kExtensionsCookieFilename);
      (*configs)[extensions::kExtensionScheme] =
          CookieStoreConfig(cookie_path, session_cookie_mode, NULL, NULL);
    }
  }
}

}  // namespace chrome_browser_net
