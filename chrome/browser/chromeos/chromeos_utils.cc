// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromeos_utils.h"

#include "base/sys_info.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {
const char* kChromeboxBoards[]  = { "stumpy", "panther" };
}

base::string16 GetChromeDeviceType() {
  std::string board = base::SysInfo::GetLsbReleaseBoard();
  for (unsigned int i = 0; i < arraysize(kChromeboxBoards); ++i) {
    std::string chromebox = kChromeboxBoards[i];
    if (board.compare(0, chromebox.length(), chromebox) == 0)
      return l10n_util::GetStringUTF16(IDS_CHROMEBOX);
  }
  return l10n_util::GetStringUTF16(IDS_CHROMEBOOK);
}

}  // namespace chromeos
