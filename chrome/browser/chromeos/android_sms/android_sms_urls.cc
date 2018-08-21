// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"

#include <string>

#include "base/command_line.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/android_sms/android_sms_switches.h"
#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

namespace {

// NOTE: Using internal staging server until changes roll out to prod.
const char kDefaultAndroidMessagesUrl[] =
    "https://android-messages.sandbox.google.com/";

// NOTE: Using experiment mods until changes roll out to prod.
const char kExperimentUrlParams[] =
    "/?e=DittoServiceWorker,DittoPwa,DittoIndexedDb";

GURL GetURLInternal(bool with_experiments) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string url_string =
      command_line->GetSwitchValueASCII(switches::kAlternateAndroidMessagesUrl);

  if (url_string.empty())
    url_string = std::string(kDefaultAndroidMessagesUrl);
  if (with_experiments)
    url_string += std::string(kExperimentUrlParams);
  return GURL(url_string);
}

}  // namespace

GURL GetAndroidMessagesURL() {
  return GetURLInternal(false /* with_experiments */);
}

GURL GetAndroidMessagesURLWithExperiments() {
  return GetURLInternal(true /* with_experiments */);
}

}  // namespace android_sms

}  // namespace chromeos
