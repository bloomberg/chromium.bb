// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/android/chromecast_config_android.h"

namespace chromecast {
namespace android {

namespace {
base::LazyInstance<ChromecastConfigAndroid> g_instance =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
ChromecastConfigAndroid* ChromecastConfigAndroid::GetInstance() {
  return g_instance.Pointer();
}

ChromecastConfigAndroid::ChromecastConfigAndroid() {
}

ChromecastConfigAndroid::~ChromecastConfigAndroid() {
}

// Registers a handler to be notified when SendUsageStats is changed.
void ChromecastConfigAndroid::SetSendUsageStatsChangedCallback(
    const base::Callback<void(bool)>& callback) {
  send_usage_stats_changed_callback_ = callback;
}

}  // namespace android
}  // namespace chromecast
