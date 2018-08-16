// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/android_sms/android_sms_switches.h"
#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

namespace {

const char kDefaultAndroidMessagesUrl[] = "https://messages.android.com";

}  // namespace

GURL GetAndroidMessagesURL() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  GURL android_messages_url(command_line->GetSwitchValueASCII(
      switches::kAlternateAndroidMessagesUrl));
  if (android_messages_url.is_empty()) {
    android_messages_url = GURL(kDefaultAndroidMessagesUrl);
  }
  return android_messages_url;
}

}  // namespace android_sms

}  // namespace chromeos
