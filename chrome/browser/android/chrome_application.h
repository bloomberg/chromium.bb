// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_APPLICATION_H_
#define CHROME_BROWSER_ANDROID_CHROME_APPLICATION_H_

#include <jni.h>

#include "base/macros.h"

namespace chrome {
namespace android {

// Represents Android Chrome Application. This is a singleton and
// provides functions to request browser side actions, such as opening a
// settings page.
class ChromeApplication {
 private:
  ChromeApplication() {}
  ~ChromeApplication() {}

  DISALLOW_COPY_AND_ASSIGN(ChromeApplication);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CHROME_APPLICATION_H_
