// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/chromeos/android_sms/android_sms_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

namespace {

const char kGoogleMessagesInstallUrl[] =
    "https://messages.google.com/web/authentication";
const char kGoogleMessagesUrl[] = "https://messages.google.com/web/";
const char kAndroidMessagesUrl[] = "https://messages.android.com/";

GURL GetAndroidMessagesURL(bool use_google_url_if_applicable,
                           bool use_install_url) {
  // If a custom URL was passed via a command line argument, use it.
  std::string url_from_command_line_arg =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAlternateAndroidMessagesUrl);
  if (!url_from_command_line_arg.empty())
    return GURL(url_from_command_line_arg);

  if (use_google_url_if_applicable) {
    return use_install_url ? GURL(kGoogleMessagesInstallUrl)
                           : GURL(kGoogleMessagesUrl);
  }
  return GURL(kAndroidMessagesUrl);
}

}  // namespace

GURL GetAndroidMessagesURL(bool use_install_url) {
  return GetAndroidMessagesURL(
      base::FeatureList::IsEnabled(features::kUseMessagesGoogleComDomain),
      use_install_url);
}

GURL GetAndroidMessagesURLOld(bool use_install_url) {
  return GetAndroidMessagesURL(
      !base::FeatureList::IsEnabled(features::kUseMessagesGoogleComDomain),
      use_install_url);
}

}  // namespace android_sms

}  // namespace chromeos
