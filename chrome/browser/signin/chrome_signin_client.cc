// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "url/gurl.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_constants.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace {

const char kGoogleAccountsUrl[] = "https://accounts.google.com";

}  // namespace

ChromeSigninClient::ChromeSigninClient(Profile* profile) : profile_(profile) {}

ChromeSigninClient::~ChromeSigninClient() {}

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
