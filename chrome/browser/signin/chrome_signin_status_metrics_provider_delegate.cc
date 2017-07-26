// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_status_metrics_provider_delegate.h"

#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#endif

ChromeSigninStatusMetricsProviderDelegate::
    ChromeSigninStatusMetricsProviderDelegate() {}

ChromeSigninStatusMetricsProviderDelegate::
    ~ChromeSigninStatusMetricsProviderDelegate() {
#if !defined(OS_ANDROID)
  BrowserList::RemoveObserver(this);
#endif

  SigninManagerFactory* factory = SigninManagerFactory::GetInstance();
  if (factory)
    factory->RemoveObserver(this);
}

void ChromeSigninStatusMetricsProviderDelegate::Initialize() {
#if !defined(OS_ANDROID)
  // On Android, there is always only one profile in any situation, opening new
  // windows (which is possible with only some Android devices) will not change
  // the opened profiles signin status.
  BrowserList::AddObserver(this);
#endif

  SigninManagerFactory* factory = SigninManagerFactory::GetInstance();
  if (factory)
    factory->AddObserver(this);
}

AccountsStatus
ChromeSigninStatusMetricsProviderDelegate::GetStatusOfAllAccounts() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profile_list = profile_manager->GetLoadedProfiles();

  AccountsStatus accounts_status;
  accounts_status.num_accounts = profile_list.size();
  for (Profile* profile : profile_list) {
#if !defined(OS_ANDROID)
    if (chrome::GetBrowserCount(profile) == 0) {
      // The profile is loaded, but there's no opened browser for this profile.
      continue;
    }
#endif
    accounts_status.num_opened_accounts++;

    SigninManager* manager =
        SigninManagerFactory::GetForProfile(profile->GetOriginalProfile());
    if (manager && manager->IsAuthenticated())
      accounts_status.num_signed_in_accounts++;
  }

  return accounts_status;
}

std::vector<SigninManager*>
ChromeSigninStatusMetricsProviderDelegate::GetSigninManagersForAllAccounts() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();

  std::vector<SigninManager*> managers;
  for (Profile* profile : profiles) {
    SigninManager* manager =
        SigninManagerFactory::GetForProfileIfExists(profile);
    if (manager)
      managers.push_back(manager);
  }

  return managers;
}

void ChromeSigninStatusMetricsProviderDelegate::OnBrowserAdded(
    Browser* browser) {
  SigninManager* manager =
      SigninManagerFactory::GetForProfile(browser->profile());

  // Nothing will change if the opened browser is in incognito mode.
  if (!manager)
    return;

  const bool signed_in = manager->IsAuthenticated();
  UpdateStatusWhenBrowserAdded(signed_in);
}

void ChromeSigninStatusMetricsProviderDelegate::SigninManagerCreated(
    SigninManagerBase* manager) {
  owner()->OnSigninManagerCreated(manager);
}

void ChromeSigninStatusMetricsProviderDelegate::SigninManagerShutdown(
    SigninManagerBase* manager) {
  owner()->OnSigninManagerShutdown(manager);
}

void ChromeSigninStatusMetricsProviderDelegate::UpdateStatusWhenBrowserAdded(
    bool signed_in) {
#if !defined(OS_ANDROID)
  SigninStatusMetricsProviderBase::SigninStatus status =
      owner()->signin_status();

  // NOTE: If |status| is MIXED_SIGNIN_STATUS, this method
  // intentionally does not update it.
  if ((status == SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN &&
       signed_in) ||
      (status == SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN &&
       !signed_in)) {
    owner()->UpdateSigninStatus(
        SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS);
  } else if (status == SigninStatusMetricsProviderBase::UNKNOWN_SIGNIN_STATUS) {
    // If when function ProvideCurrentSessionData() is called, Chrome is
    // running in the background with no browser window opened, |signin_status_|
    // will be reset to |UNKNOWN_SIGNIN_STATUS|. Then this newly added browser
    // is the only opened browser/profile and its signin status represents
    // the whole status.
    owner()->UpdateSigninStatus(
        signed_in
            ? SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN
            : SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN);
  }
#endif
}
