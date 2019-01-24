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

// Returns the old URL used for Android Messages. In this context, the "old" URL
// refers to the URL used before the last change to the
// kUseMessagesGoogleComDomain flag. See go/awm-cros-domain for details.
// TODO(https://crbug.com/917855): Remove this function when migration is
// complete.
GURL GetAndroidMessagesURLOld();

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_URLS_H_
