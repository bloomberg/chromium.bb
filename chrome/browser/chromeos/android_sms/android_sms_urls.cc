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

const char kAndroidMessagesUrl[] = "https://messages.android.com/";
const char kGoogleMessagesUrl[] = "https://messages.google.com/";

GURL GetAndroidMessagesURL(bool use_google_url_if_applicable) {
  // If a custom URL was passed via a command line argument, use it.
  std::string url_from_command_line_arg =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAlternateAndroidMessagesUrl);
  if (!url_from_command_line_arg.empty())
    return GURL(url_from_command_line_arg);

  return use_google_url_if_applicable ? GURL(kGoogleMessagesUrl)
                                      : GURL(kAndroidMessagesUrl);
}

}  // namespace

GURL GetAndroidMessagesURL() {
  return GetAndroidMessagesURL(
      base::FeatureList::IsEnabled(features::kUseMessagesGoogleComDomain));
}

GURL GetAndroidMessagesURLOld() {
  return GetAndroidMessagesURL(
      !base::FeatureList::IsEnabled(features::kUseMessagesGoogleComDomain));
}

}  // namespace android_sms

}  // namespace chromeos
