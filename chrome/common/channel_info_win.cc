// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/debug/profiler.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/install_static/install_util.h"

namespace chrome {

std::string GetChannelName() {
#if defined(GOOGLE_CHROME_BUILD)
  base::string16 channel(install_static::GetChromeChannelName());
#if defined(SYZYASAN)
  if (base::debug::IsBinaryInstrumented())
    channel += L" SyzyASan";
#endif
  return base::UTF16ToASCII(channel);
#else
  return std::string();
#endif
}

version_info::Channel GetChannel() {
  return install_static::GetChromeChannel();
}

}  // namespace chrome
