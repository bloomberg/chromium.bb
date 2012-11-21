// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/udev_info_provider.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/system/name_value_pairs_parser.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace system {

namespace {

// Absolute path of the udevadm tool.
const char kUdevadmToolPath[] = "/sbin/udevadm";
// Key-value separator and string delimiter in output of "udevadm info".
const char kUdevadmEq[] = "=";
const char kUdevadmDelim[] = "\n";
const char kUdevadmCommentDelim[] = "";

}  // namespace

bool UdevInfoProvider::QueryDeviceProperty(const std::string& sys_path,
                                           const std::string& property_name,
                                           std::string* result) {
  const char* kUdevadmTool[] = {
    kUdevadmToolPath,
    "info",
    "--path",
    sys_path.c_str(),
    "--query=property",
  };

  NameValuePairsParser::NameValueMap device_info;
  NameValuePairsParser parser(&device_info);

  if (!parser.ParseNameValuePairsFromTool(
          arraysize(kUdevadmTool), kUdevadmTool, kUdevadmEq,
          kUdevadmDelim, kUdevadmCommentDelim)) {
    LOG(WARNING) << "There were errors parsing the output of "
                 << kUdevadmToolPath << ".";
  }

  NameValuePairsParser::NameValueMap::const_iterator it =
      device_info.find(property_name);
  if (it == device_info.end())
    return false;

  *result = it->second;
  return true;
}

}  // namespace system
}  // namespace chromeos
