// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INFO_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INFO_H_

#include <string>

#include "base/macros.h"

// Structure with information about a WebAPK.
//
// This class is passed around in a std::vector to generate the chrome://webapks
// page. To reduce copying overhead, this class is move-only, and
// move-constructs its string arguments (which are copied from Java to C++ into
// a temporary prior to construction).
struct WebApkInfo {
  WebApkInfo(std::string short_name,
             std::string package_name,
             int shell_apk_version,
             int version_code);
  ~WebApkInfo();

  WebApkInfo& operator=(WebApkInfo&& other) = default;
  WebApkInfo(WebApkInfo&& other) = default;

  // Short name of the WebAPK.
  std::string short_name;

  // Package name of the WebAPK.
  std::string package_name;

  // Shell APK version of the WebAPK.
  int shell_apk_version;

  // Version code of the WebAPK.
  int version_code;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebApkInfo);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INFO_H_
