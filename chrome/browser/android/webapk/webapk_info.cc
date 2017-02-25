// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_info.h"

WebApkInfo::WebApkInfo(std::string short_name,
                       std::string package_name,
                       int shell_apk_version,
                       int version_code)
    : short_name(std::move(short_name)),
      package_name(std::move(package_name)),
      shell_apk_version(shell_apk_version),
      version_code(version_code) {}

WebApkInfo::~WebApkInfo() {}
