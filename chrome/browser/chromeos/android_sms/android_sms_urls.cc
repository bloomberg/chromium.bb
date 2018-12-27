// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"

#include <string>

#include "base/command_line.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/android_sms/android_sms_switches.h"
#include "chromeos/chromeos_features.h"
#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

namespace {

const char kAndroidMessagesProdUrl[] = "https://messages.android.com/";

}  // namespace

GURL GetAndroidMessagesURL() {
  std::string url_from_command_line_arg =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAlternateAndroidMessagesUrl);

  if (!url_from_command_line_arg.empty())
    return GURL(url_from_command_line_arg);

  return GURL(kAndroidMessagesProdUrl);
}

}  // namespace android_sms

}  // namespace chromeos
