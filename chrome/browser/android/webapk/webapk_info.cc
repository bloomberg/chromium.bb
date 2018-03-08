// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/android/webapk/webapk_info.h"

WebApkInfo::WebApkInfo(std::string name,
                       std::string short_name,
                       std::string package_name,
                       int shell_apk_version,
                       int version_code,
                       std::string uri,
                       std::string scope,
                       std::string manifest_url,
                       std::string manifest_start_url,
                       blink::WebDisplayMode display,
                       blink::WebScreenOrientationLockType orientation,
                       int64_t theme_color,
                       int64_t background_color,
                       base::Time last_update_check_time,
                       bool relax_updates)
    : name(std::move(name)),
      short_name(std::move(short_name)),
      package_name(std::move(package_name)),
      shell_apk_version(shell_apk_version),
      version_code(version_code),
      uri(std::move(uri)),
      scope(std::move(scope)),
      manifest_url(std::move(manifest_url)),
      manifest_start_url(std::move(manifest_start_url)),
      display(display),
      orientation(orientation),
      theme_color(theme_color),
      background_color(background_color),
      last_update_check_time(last_update_check_time),
      relax_updates(relax_updates) {}

WebApkInfo::~WebApkInfo() {}

WebApkInfo& WebApkInfo::operator=(WebApkInfo&& rhs) = default;
WebApkInfo::WebApkInfo(WebApkInfo&& other) = default;
