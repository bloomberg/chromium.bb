// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/chromecast_config_android.h"

namespace chromecast {
namespace android {

// Dummy implementation of android::ChromecastConfigAndroid.
class ChromecastConfigAndroidDummy : public ChromecastConfigAndroid {
 public:
  ChromecastConfigAndroidDummy() {}

  ~ChromecastConfigAndroidDummy() override {}

  bool CanSendUsageStats() override { return false; }

  void SetSendUsageStats(bool enabled) override {}

  void SetSendUsageStatsChangedCallback(
      base::RepeatingCallback<void(bool)> callback) override {}

  void RunSendUsageStatsChangedCallback(bool enabled) override {}

 private:
  friend class base::NoDestructor<ChromecastConfigAndroidDummy>;

  DISALLOW_COPY_AND_ASSIGN(ChromecastConfigAndroidDummy);
};

// static
ChromecastConfigAndroid* ChromecastConfigAndroid::GetInstance() {
  static base::NoDestructor<ChromecastConfigAndroidDummy> instance;
  return instance.get();
}

}  // namespace android
}  // namespace chromecast
