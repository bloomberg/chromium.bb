// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_version_info.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"

namespace chrome {

// static
std::string VersionInfo::GetVersionStringModifier() {
#if defined(GOOGLE_CHROME_BUILD)
  base::FilePath module;
  string16 channel;
  if (PathService::Get(base::FILE_MODULE, &module)) {
    bool is_system_install =
        !InstallUtil::IsPerUserInstall(module.value().c_str());

    GoogleUpdateSettings::GetChromeChannelAndModifiers(is_system_install,
                                                       &channel);
  }
  return UTF16ToASCII(channel);
#else
  return std::string();
#endif
}

// static
VersionInfo::Channel VersionInfo::GetChannel() {
#if defined(GOOGLE_CHROME_BUILD)
  std::wstring channel(L"unknown");

  base::FilePath module;
  if (PathService::Get(base::FILE_MODULE, &module)) {
    bool is_system_install =
        !InstallUtil::IsPerUserInstall(module.value().c_str());
    channel = GoogleUpdateSettings::GetChromeChannel(is_system_install);
  }

  if (channel.empty()) {
    return CHANNEL_STABLE;
  } else if (channel == L"beta") {
    return CHANNEL_BETA;
  } else if (channel == L"dev") {
    return CHANNEL_DEV;
  } else if (channel == L"canary") {
    return CHANNEL_CANARY;
  }
#endif

  return CHANNEL_UNKNOWN;
}

}  // namespace chrome
