// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ACTIVITY_METRICS_RECORDER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ACTIVITY_METRICS_RECORDER_H_

#include <stddef.h>

#include "base/macros.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

namespace base {
template <typename T>
class NoDestructor;
}

class ProfileActivityMetricsRecorder : public BrowserListObserver {
 public:
  // Initializes a |ProfileActivityMetricsRecorder| object and starts
  // tracking/recording.
  static void Initialize();

  // BrowserListObserver overrides:
  void OnBrowserSetLastActive(Browser* browser) override;

 private:
  friend class base::NoDestructor<ProfileActivityMetricsRecorder>;
  ProfileActivityMetricsRecorder();

  DISALLOW_COPY_AND_ASSIGN(ProfileActivityMetricsRecorder);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ACTIVITY_METRICS_RECORDER_H_
