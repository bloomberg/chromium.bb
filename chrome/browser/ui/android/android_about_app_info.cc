// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/android_about_app_info.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "v8/include/v8.h"

std::string AndroidAboutAppInfo::GetOsInfo() {
  std::string android_info_str;

  // Append information about the OS version.
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
  base::StringAppendF(&android_info_str, "%d.%d.%d", os_major_version,
                      os_minor_version, os_bugfix_version);

  // Append information about the device.
  bool semicolon_inserted = false;
  std::string android_build_codename = base::SysInfo::GetAndroidBuildCodename();
  std::string android_device_name = base::SysInfo::GetDeviceName();
  if ("REL" == android_build_codename && android_device_name.size() > 0) {
    android_info_str += "; " + android_device_name;
    semicolon_inserted = true;
  }

  // Append the build ID.
  std::string android_build_id = base::SysInfo::GetAndroidBuildID();
  if (android_build_id.size() > 0) {
    if (!semicolon_inserted) {
      android_info_str += ";";
    }
    android_info_str += " Build/" + android_build_id;
  }

  return android_info_str;
}

std::string AndroidAboutAppInfo::GetJavaScriptVersion() {
  std::string js_version(v8::V8::GetVersion());
  std::string js_engine = "V8";
  return js_engine + " " + js_version;
}
