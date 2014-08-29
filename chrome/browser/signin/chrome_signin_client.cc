// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include "base/command_line.h"
#include "base/guid.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/chrome_version_info.h"
#include "components/metrics/metrics_service.h"
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
#include "components/user_manager/user_manager.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/first_run/first_run.h"
#endif

using content::ChildProcessHost;
using content::RenderProcessHost;

namespace {

const char kGoogleAccountsUrl[] = "https://accounts.google.com";

}  // namespace

ChromeSigninClient::ChromeSigninClient(Profile* profile)
    : profile_(profile), signin_host_id_(ChildProcessHost::kInvalidUniqueID) {
  callbacks_.set_removal_callback(
    base::Bind(&ChromeSigninClient::UnregisterForCookieChangedNotification,
               base::Unretained(this)));
}

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
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser()) {
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
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSigninScopedDeviceId)) {
    return std::string();
  }

  std::string signin_scoped_device_id =
      GetPrefs()->GetString(prefs::kGoogleServicesSigninScopedDeviceId);
  if (signin_scoped_device_id.empty()) {
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
  // If inline sign in is enabled, but account consistency is not, the user's
  // credentials should be merge into the cookie jar.
  return !switches::IsEnableWebBasedSignin() &&
         !switches::IsEnableAccountConsistency();
}

std::string ChromeSigninClient::GetProductVersion() {
  chrome::VersionInfo chrome_version;
  if (!chrome_version.is_valid())
    return "invalid";
  return chrome_version.CreateVersionString();
}

bool ChromeSigninClient::IsFirstRun() const {
#if defined(OS_ANDROID)
  return false;
#else
  return first_run::IsChromeFirstRun();
#endif
}

base::Time ChromeSigninClient::GetInstallDate() {
  return base::Time::FromTimeT(
      g_browser_process->metrics_service()->GetInstallDate());
}

scoped_ptr<SigninClient::CookieChangedCallbackList::Subscription>
ChromeSigninClient::AddCookieChangedCallback(
    const CookieChangedCallback& callback) {
  scoped_ptr<SigninClient::CookieChangedCallbackList::Subscription>
    subscription = callbacks_.Add(callback);
  RegisterForCookieChangedNotification();
  return subscription.Pass();
}

void ChromeSigninClient::GoogleSigninSucceeded(const std::string& account_id,
                                               const std::string& username,
                                               const std::string& password) {
#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
  // Don't store password hash except for users of new profile management.
  if (switches::IsNewProfileManagement())
    chrome::SetLocalAuthCredentials(profile_, password);
#endif
}

void ChromeSigninClient::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_COOKIE_CHANGED: {
      DCHECK(!callbacks_.empty());
      const net::CanonicalCookie* cookie =
          content::Details<ChromeCookieDetails>(details).ptr()->cookie;
      callbacks_.Notify(cookie);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void ChromeSigninClient::RegisterForCookieChangedNotification() {
  if (callbacks_.empty())
    return;
  content::Source<Profile> source(profile_);
  if (!registrar_.IsRegistered(
      this, chrome::NOTIFICATION_COOKIE_CHANGED, source))
  registrar_.Add(this, chrome::NOTIFICATION_COOKIE_CHANGED, source);
}

void ChromeSigninClient::UnregisterForCookieChangedNotification() {
  if (!callbacks_.empty())
    return;
  // Note that it's allowed to call this method multiple times without an
  // intervening call to |RegisterForCookieChangedNotification()|.
  content::Source<Profile> source(profile_);
  if (!registrar_.IsRegistered(
      this, chrome::NOTIFICATION_COOKIE_CHANGED, source))
    return;
  registrar_.Remove(this, chrome::NOTIFICATION_COOKIE_CHANGED, source);
}
