// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/android/chromecast_config_android.h"

namespace chromecast {
namespace android {

bool ChromecastConfigAndroid::CanSendUsageStats() {
  return false;
}

}  // namespace android
}  // namespace chromecast
