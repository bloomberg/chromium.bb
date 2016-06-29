// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_ANDROID_MANAGEMENT_CHECKER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_ANDROID_MANAGEMENT_CHECKER_DELEGATE_H_

#include "chrome/browser/chromeos/policy/android_management_client.h"

namespace arc {

class ArcAndroidManagementCheckerDelegate {
 public:
  virtual void OnAndroidManagementChecked(
      policy::AndroidManagementClient::Result result) = 0;

 protected:
  virtual ~ArcAndroidManagementCheckerDelegate() {}
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_ANDROID_MANAGEMENT_CHECKER_DELEGATE_H_
