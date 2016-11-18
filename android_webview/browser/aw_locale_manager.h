// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_LOCALE_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_LOCALE_MANAGER_H_

#include <string>

#include "base/macros.h"

namespace android_webview {

// Empty base class so this can be destroyed by AwContentBrowserClient.
class AwLocaleManager {
 public:
  AwLocaleManager() {}
  virtual ~AwLocaleManager() {}

  virtual std::string GetLocale() = 0;

  virtual std::string GetLocaleList() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AwLocaleManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_LOCALE_MANAGER_H_
