// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_url.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"

const char kDefaultCloudPrintServiceURL[] = "https://www.google.com/cloudprint";
const char kLearnMoreURL[] =
    "https://www.google.com/support/cloudprint";
const char kTestPageURL[] =
    "http://www.google.com/landing/cloudprint/enable.html?print=true";

void CloudPrintURL::RegisterPreferences() {
  DCHECK(profile_);
  PrefService* pref_service = profile_->GetPrefs();
  if (pref_service->FindPreference(prefs::kCloudPrintServiceURL))
    return;
  pref_service->RegisterStringPref(prefs::kCloudPrintServiceURL,
                                   kDefaultCloudPrintServiceURL);
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

GURL CloudPrintURL::GetCloudPrintServiceDialogURL() {
  GURL cloud_print_service_url = GetCloudPrintServiceURL();
  std::string path(cloud_print_service_url.path() + "/client/dialog.html");
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  GURL cloud_print_dialog_url = cloud_print_service_url.ReplaceComponents(
      replacements);
  return cloud_print_dialog_url;
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

GURL CloudPrintURL::GetCloudPrintLearnMoreURL() {
  GURL cloud_print_learn_more_url(kLearnMoreURL);
  return cloud_print_learn_more_url;
}

GURL CloudPrintURL::GetCloudPrintTestPageURL() {
  GURL cloud_print_learn_more_url(kTestPageURL);
  return cloud_print_learn_more_url;
}
