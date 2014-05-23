// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_utils.h"

#include "base/logging.h"
#include "chrome/common/chrome_version_info.h"

namespace gcm {

namespace {

GCMClient::ChromePlatform GetPlatform() {
#if defined(OS_WIN)
  return GCMClient::PLATFORM_WIN;
#elif defined(OS_MACOSX)
  return GCMClient::PLATFORM_MAC;
#elif defined(OS_IOS)
  return GCMClient::PLATFORM_IOS;
#elif defined(OS_ANDROID)
  return GCMClient::PLATFORM_ANDROID;
#elif defined(OS_CHROMEOS)
  return GCMClient::PLATFORM_CROS;
#elif defined(OS_LINUX)
  return GCMClient::PLATFORM_LINUX;
#else
  // For all other platforms, return as LINUX.
  return GCMClient::PLATFORM_LINUX;
#endif
}

GCMClient::ChromeChannel GetChannel() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return GCMClient::CHANNEL_UNKNOWN;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return GCMClient::CHANNEL_CANARY;
    case chrome::VersionInfo::CHANNEL_DEV:
      return GCMClient::CHANNEL_DEV;
    case chrome::VersionInfo::CHANNEL_BETA:
      return GCMClient::CHANNEL_BETA;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return GCMClient::CHANNEL_STABLE;
    default:
      NOTREACHED();
      return GCMClient::CHANNEL_UNKNOWN;
  }
}

std::string GetVersion() {
  chrome::VersionInfo version_info;
  return version_info.Version();
}

}  // namespace

GCMClient::ChromeBuildInfo GetChromeBuildInfo() {
  GCMClient::ChromeBuildInfo chrome_build_info;
  chrome_build_info.platform = GetPlatform();
  chrome_build_info.channel = GetChannel();
  chrome_build_info.version = GetVersion();
  return chrome_build_info;
}

}  // namespace gcm
