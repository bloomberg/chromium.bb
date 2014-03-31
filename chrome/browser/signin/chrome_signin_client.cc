// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/profile_management_switches.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "url/gurl.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_constants.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

using content::ChildProcessHost;
using content::RenderProcessHost;

namespace {

const char kGoogleAccountsUrl[] = "https://accounts.google.com";

}  // namespace

ChromeSigninClient::ChromeSigninClient(Profile* profile)
    : profile_(profile), signin_host_id_(ChildProcessHost::kInvalidUniqueID) {}

ChromeSigninClient::~ChromeSigninClient() {
  std::set<RenderProcessHost*>::iterator i;
  for (i = signin_hosts_observed_.begin(); i != signin_hosts_observed_.end();
       ++i) {
    (*i)->RemoveObserver(this);
  }
}

// static
bool ChromeSigninClient::ProfileAllowsSigninCookies(Profile* profile) {
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(profile).get();
  return SettingsAllowSigninCookies(cookie_settings);
}

// static
bool ChromeSigninClient::SettingsAllowSigninCookies(
    CookieSettings* cookie_settings) {
  return cookie_settings &&
         cookie_settings->IsSettingCookieAllowed(GURL(kGoogleAccountsUrl),
                                                 GURL(kGoogleAccountsUrl));
}

void ChromeSigninClient::SetSigninProcess(int process_id) {
  if (process_id == signin_host_id_)
    return;
  DLOG_IF(WARNING, signin_host_id_ != ChildProcessHost::kInvalidUniqueID)
      << "Replacing in-use signin process.";
  signin_host_id_ = process_id;
  RenderProcessHost* host = RenderProcessHost::FromID(process_id);
  DCHECK(host);
  host->AddObserver(this);
  signin_hosts_observed_.insert(host);
}

void ChromeSigninClient::ClearSigninProcess() {
  signin_host_id_ = ChildProcessHost::kInvalidUniqueID;
}

bool ChromeSigninClient::IsSigninProcess(int process_id) const {
  return process_id == signin_host_id_;
}

bool ChromeSigninClient::HasSigninProcess() const {
  return signin_host_id_ != ChildProcessHost::kInvalidUniqueID;
}

void ChromeSigninClient::RenderProcessHostDestroyed(RenderProcessHost* host) {
  // It's possible we're listening to a "stale" renderer because it was replaced
  // with a new process by process-per-site. In either case, stop observing it,
  // but only reset signin_host_id_ tracking if this was from the current signin
  // process.
  signin_hosts_observed_.erase(host);
  if (signin_host_id_ == host->GetID())
    signin_host_id_ = ChildProcessHost::kInvalidUniqueID;
}

PrefService* ChromeSigninClient::GetPrefs() { return profile_->GetPrefs(); }

scoped_refptr<TokenWebData> ChromeSigninClient::GetDatabase() {
  return WebDataServiceFactory::GetTokenWebDataForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
}

bool ChromeSigninClient::CanRevokeCredentials() {
#if defined(OS_CHROMEOS)
  // UserManager may not exist in unit_tests.
  if (chromeos::UserManager::IsInitialized() &&
      chromeos::UserManager::Get()->IsLoggedInAsLocallyManagedUser()) {
    // Don't allow revoking credentials for Chrome OS supervised users.
    // See http://crbug.com/332032
    LOG(ERROR) << "Attempt to revoke supervised user refresh "
               << "token detected, ignoring.";
    return false;
  }
#else
  // Don't allow revoking credentials for supervised users.
  // See http://crbug.com/332032
  if (profile_->IsManaged()) {
    LOG(ERROR) << "Attempt to revoke supervised user refresh "
               << "token detected, ignoring.";
    return false;
  }
#endif
  return true;
}

net::URLRequestContextGetter* ChromeSigninClient::GetURLRequestContext() {
  return profile_->GetRequestContext();
}

void ChromeSigninClient::GoogleSigninSucceeded(const std::string& username,
                                               const std::string& password) {
#if !defined(OS_ANDROID)
  // Don't store password hash except for users of new profile features.
  if (switches::IsNewProfileManagement())
    chrome::SetLocalAuthCredentials(profile_, password);
#endif
}
