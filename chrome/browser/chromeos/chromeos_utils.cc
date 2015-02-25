// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromeos_utils.h"

#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// List of ChromeOS board names corresponding to Chromebase devices. Googlers
// can find a list of ChromeOS device and board names at http://go/cros-names
const char* const kChromebaseBoards[]  = {
  "monroe",
};

// List of ChromeOS board names corresponding to Chromebox devices. Googlers
// can find a list of ChromeOS device and board names at http://go/cros-names
const char* const kChromeboxBoards[]  = {
  "panther",
  "stumpy",
  "zako",
};

}  // namespace

namespace chrome_device_types {

const char kChromebox[] = "chromebox";
const char kChromebase[] = "chromebase";
const char kChromebook[] = "chromebook";

}  // namespace chrome_device_types

base::string16 GetChromeDeviceType() {
  return l10n_util::GetStringUTF16(GetChromeDeviceTypeResourceId());
}

int GetChromeDeviceTypeResourceId() {
  const std::string board = base::SysInfo::GetLsbReleaseBoard();
  for (size_t i = 0; i < arraysize(kChromeboxBoards); ++i) {
    if (StartsWithASCII(board, kChromeboxBoards[i], true))
      return IDS_CHROMEBOX;
  }
  for (size_t i = 0; i < arraysize(kChromebaseBoards); ++i) {
    if (StartsWithASCII(board, kChromebaseBoards[i], true))
      return IDS_CHROMEBASE;
  }
  return IDS_CHROMEBOOK;
}

std::string GetChromeDeviceTypeString() {
  int resource_id = GetChromeDeviceTypeResourceId();
  switch (resource_id) {
    case IDS_CHROMEBOX:
      return chrome_device_types::kChromebox;
    case IDS_CHROMEBASE:
      return chrome_device_types::kChromebase;
    default:
      NOTREACHED() << "Unknown Chrome device type: " << resource_id;
    case IDS_CHROMEBOOK:
      return chrome_device_types::kChromebook;
  }
}

}  // namespace chromeos
