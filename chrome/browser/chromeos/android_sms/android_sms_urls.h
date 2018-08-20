// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_URLS_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_URLS_H_

#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

// Returns URL to Android Messages for Web page used by AndroidSmsService.
GURL GetAndroidMessagesURL();

// Returns URL to Android Messages for Web page used by AndroidSmsService.
// Includes the experiment URL params for the ChromeOS integrations.
// This is temporary for dogfood until these flags are rolled out to prod.
GURL GetAndroidMessagesURLWithExperiments();

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_URLS_H_
