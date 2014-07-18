// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include "base/command_line.h"
#include "base/guid.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/chrome_version_info.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/signin/core/common/signin_switches.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "url/gurl.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/user_manager.h"
#endif

using content::ChildProcessHost;
using content::RenderProcessHost;

namespace {

const char kGoogleAccountsUrl[] = "https://accounts.google.com";

}  // namespace

ChromeSigninClient::ChromeSigninClient(Profile* profile)
    : profile_(profile), signin_host_id_(ChildProcessHost::kInvalidUniqueID) {}

ChromeSigninClient::~ChromeSigninClient() {
  UnregisterForCookieChangedNotification();

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
  return process_id != ChildProcessHost::kInvalidUniqueID &&
         process_id == signin_host_id_;
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
      chromeos::UserManager::Get()->IsLoggedInAsSupervisedUser()) {
    // Don't allow revoking credentials for Chrome OS supervised users.
    // See http://crbug.com/332032
    LOG(ERROR) << "Attempt to revoke supervised user refresh "
               << "token detected, ignoring.";
    return false;
  }
#else
  // Don't allow revoking credentials for supervised users.
  // See http://crbug.com/332032
  if (profile_->IsSupervised()) {
    LOG(ERROR) << "Attempt to revoke supervised user refresh "
               << "token detected, ignoring.";
    return false;
  }
#endif
  return true;
}

std::string ChromeSigninClient::GetSigninScopedDeviceId() {
  std::string signin_scoped_device_id =
      GetPrefs()->GetString(prefs::kGoogleServicesSigninScopedDeviceId);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSigninScopedDeviceId) &&
      signin_scoped_device_id.empty()) {
    // If device_id doesn't exist then generate new and save in prefs.
    signin_scoped_device_id = base::GenerateGUID();
    DCHECK(!signin_scoped_device_id.empty());
    GetPrefs()->SetString(prefs::kGoogleServicesSigninScopedDeviceId,
                          signin_scoped_device_id);
  }
  return signin_scoped_device_id;
}

void ChromeSigninClient::ClearSigninScopedDeviceId() {
  GetPrefs()->ClearPref(prefs::kGoogleServicesSigninScopedDeviceId);
}

net::URLRequestContextGetter* ChromeSigninClient::GetURLRequestContext() {
  return profile_->GetRequestContext();
}

bool ChromeSigninClient::ShouldMergeSigninCredentialsIntoCookieJar() {
  // If inline sign in is enabled, but new profile management is not, the user's
  // credentials should be merge into the cookie jar.
  return !switches::IsEnableWebBasedSignin() &&
         !switches::IsNewProfileManagement();
}

std::string ChromeSigninClient::GetProductVersion() {
  chrome::VersionInfo chrome_version;
  if (!chrome_version.is_valid())
    return "invalid";
  return chrome_version.CreateVersionString();
}

void ChromeSigninClient::SetCookieChangedCallback(
    const CookieChangedCallback& callback) {
  if (callback_.Equals(callback))
    return;

  // There should be only one callback active at a time.
  DCHECK(callback.is_null() || callback_.is_null());
  callback_ = callback;
  if (!callback_.is_null())
    RegisterForCookieChangedNotification();
  else
    UnregisterForCookieChangedNotification();
}

void ChromeSigninClient::GoogleSigninSucceeded(const std::string& username,
                                               const std::string& password) {
#if !defined(OS_ANDROID)
  // Don't store password hash except for users of new profile features.
  if (switches::IsNewProfileManagement())
    chrome::SetLocalAuthCredentials(profile_, password);
#endif
}

void ChromeSigninClient::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_COOKIE_CHANGED: {
      DCHECK(!callback_.is_null());
      const net::CanonicalCookie* cookie =
          content::Details<ChromeCookieDetails>(details).ptr()->cookie;
      callback_.Run(cookie);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void ChromeSigninClient::RegisterForCookieChangedNotification() {
  content::Source<Profile> source(profile_);
  DCHECK(!registrar_.IsRegistered(
      this, chrome::NOTIFICATION_COOKIE_CHANGED, source));
  registrar_.Add(this, chrome::NOTIFICATION_COOKIE_CHANGED, source);
}

void ChromeSigninClient::UnregisterForCookieChangedNotification() {
  // Note that it's allowed to call this method multiple times without an
  // intervening call to |RegisterForCookieChangedNotification()|.
  content::Source<Profile> source(profile_);
  if (!registrar_.IsRegistered(
      this, chrome::NOTIFICATION_COOKIE_CHANGED, source))
    return;
  registrar_.Remove(this, chrome::NOTIFICATION_COOKIE_CHANGED, source);
}
