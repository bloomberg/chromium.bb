// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_manager_delegate.h"

#include "chrome/browser/content_settings/cookie_settings.h"
#include "googleurl/src/gurl.h"

namespace {

const char kGoogleAccountsUrl[] = "https://accounts.google.com";

}  // namespace

ChromeSigninManagerDelegate::ChromeSigninManagerDelegate(Profile* profile)
    : profile_(profile) {
}

ChromeSigninManagerDelegate::~ChromeSigninManagerDelegate() {
}

// static
bool ChromeSigninManagerDelegate::ProfileAllowsSigninCookies(Profile* profile) {
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(profile);
  return SettingsAllowSigninCookies(cookie_settings);
}

// static
bool ChromeSigninManagerDelegate::SettingsAllowSigninCookies(
    CookieSettings* cookie_settings) {
  return cookie_settings &&
      cookie_settings->IsSettingCookieAllowed(GURL(kGoogleAccountsUrl),
                                              GURL(kGoogleAccountsUrl));
}


bool ChromeSigninManagerDelegate::AreSigninCookiesAllowed() {
  return ProfileAllowsSigninCookies(profile_);
}
