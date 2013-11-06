// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/common/chrome_version_info.h"

namespace gcm {

// static
bool GCMProfileService::IsGCMEnabled() {
  // GCM support is only enabled for Canary/Dev builds.
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  return channel == chrome::VersionInfo::CHANNEL_CANARY ||
         channel == chrome::VersionInfo::CHANNEL_DEV;
}

GCMProfileService::GCMProfileService(Profile* profile)
    : profile_(profile) {
}

GCMProfileService::~GCMProfileService() {
}

}  // namespace gcm
