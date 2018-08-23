// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CHROMECAST_CONFIG_ANDROID_H_
#define CHROMECAST_BASE_CHROMECAST_CONFIG_ANDROID_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/no_destructor.h"

namespace chromecast {
namespace android {

class ChromecastConfigAndroid {
 public:
  static ChromecastConfigAndroid* GetInstance();

  // Returns whether or not the user has allowed sending usage stats and
  // crash reports.
  virtual bool CanSendUsageStats() = 0;

  // Registers a handler to be notified when SendUsageStats is changed.
  virtual void SetSendUsageStatsChangedCallback(
      base::RepeatingCallback<void(bool)> callback) = 0;

  virtual void RunSendUsageStatsChangedCallback(bool enabled) = 0;

 protected:
  ChromecastConfigAndroid() {}

  virtual ~ChromecastConfigAndroid() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromecastConfigAndroid);
};

}  // namespace android
}  // namespace chromecast

#endif  // CHROMECAST_BASE_CHROMECAST_CONFIG_ANDROID_H_
