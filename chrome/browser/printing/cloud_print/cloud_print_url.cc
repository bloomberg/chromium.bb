// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_url.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/stringprintf.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/gaia_urls.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

// Url must not be matched by "urls" section of
// cloud_print_app/manifest.json. If it's matched, print driver dialog will
// open sign-in page in separate window.
const char kDefaultCloudPrintServiceURL[] = "https://www.google.com/cloudprint";

// Continue url must be any url matched by "urls" section of
// cloud_print_app/manifest.json.
// Current implementation of sign-in waits when browser will try to open url in
// new tab. This happens if url is "app".
// TODO(vitalybuka): Find better way to detect successful sign-in.
const char kDefaultSignInContinueURL[] =
    "https://www.google.com/cloudprint/enable_chrome_connector";

const char kLearnMoreURL[] =
    "https://www.google.com/support/cloudprint";
const char kTestPageURL[] =
    "http://www.google.com/landing/cloudprint/enable.html?print=true";

void CloudPrintURL::RegisterPreferences() {
  DCHECK(profile_);
  PrefService* pref_service = profile_->GetPrefs();
  // TODO(joi): Do all registration up front.
  scoped_refptr<PrefRegistrySyncable> registry(
      static_cast<PrefRegistrySyncable*>(
          pref_service->DeprecatedGetPrefRegistry()));
  if (!pref_service->FindPreference(prefs::kCloudPrintServiceURL)) {
    registry->RegisterStringPref(prefs::kCloudPrintServiceURL,
                                 kDefaultCloudPrintServiceURL,
                                 PrefRegistrySyncable::UNSYNCABLE_PREF);
  }
  if (!pref_service->FindPreference(prefs::kCloudPrintSigninURL)) {
    std::string url = GaiaUrls::GetInstance()->service_login_url();
    url.append("?service=cloudprint&sarp=1&continue=");
    url.append(net::EscapeQueryParamValue(kDefaultSignInContinueURL, false));
    registry->RegisterStringPref(prefs::kCloudPrintSigninURL, url,
                                 PrefRegistrySyncable::UNSYNCABLE_PREF);
  }
}

// Returns the root service URL for the cloud print service.  The default is to
// point at the Google Cloud Print service.  This can be overridden by the
// command line or by the user preferences.
GURL CloudPrintURL::GetCloudPrintServiceURL() {
  DCHECK(profile_);
  RegisterPreferences();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  GURL cloud_print_service_url = GURL(command_line.GetSwitchValueASCII(
      switches::kCloudPrintServiceURL));
  if (cloud_print_service_url.is_empty()) {
    cloud_print_service_url = GURL(
        profile_->GetPrefs()->GetString(prefs::kCloudPrintServiceURL));
  }
  return cloud_print_service_url;
}

GURL CloudPrintURL::GetCloudPrintSigninURL() {
  DCHECK(profile_);
  RegisterPreferences();

  GURL cloud_print_signin_url = GURL(
      profile_->GetPrefs()->GetString(prefs::kCloudPrintSigninURL));
  return google_util::AppendGoogleLocaleParam(cloud_print_signin_url);
}

GURL CloudPrintURL::GetCloudPrintServiceDialogURL() {
  GURL cloud_print_service_url = GetCloudPrintServiceURL();
  std::string path(cloud_print_service_url.path() + "/client/dialog.html");
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  GURL cloud_print_dialog_url = cloud_print_service_url.ReplaceComponents(
      replacements);
  return google_util::AppendGoogleLocaleParam(cloud_print_dialog_url);
}

GURL CloudPrintURL::GetCloudPrintServiceManageURL() {
  GURL cloud_print_service_url = GetCloudPrintServiceURL();
  std::string path(cloud_print_service_url.path() + "/manage.html");
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  GURL cloud_print_manage_url = cloud_print_service_url.ReplaceComponents(
      replacements);
  return cloud_print_manage_url;
}

GURL CloudPrintURL::GetCloudPrintServiceEnableURL(
    const std::string& proxy_id) {
  GURL cloud_print_service_url = GetCloudPrintServiceURL();
  std::string path(cloud_print_service_url.path() +
      "/enable_chrome_connector/enable.html");
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query = StringPrintf("proxy=%s", proxy_id.c_str());
  replacements.SetQueryStr(query);
  GURL cloud_print_enable_url = cloud_print_service_url.ReplaceComponents(
      replacements);
  return cloud_print_enable_url;
}

GURL CloudPrintURL::GetCloudPrintLearnMoreURL() {
  GURL cloud_print_learn_more_url(kLearnMoreURL);
  return cloud_print_learn_more_url;
}

GURL CloudPrintURL::GetCloudPrintTestPageURL() {
  GURL cloud_print_learn_more_url(kTestPageURL);
  return cloud_print_learn_more_url;
}
