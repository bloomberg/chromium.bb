// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_KEY_SYSTEMS_COMMON_H_
#define CHROMECAST_MEDIA_BASE_KEY_SYSTEMS_COMMON_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "chromecast/public/media/cast_key_system.h"
#include "media/base/android/media_client_android.h"

namespace chromecast {
namespace media {

#if defined(PLAYREADY_CDM_AVAILABLE)
extern const char kChromecastPlayreadyKeySystem[];
#endif  // defined(PLAYREADY_CDM_AVAILABLE)

// Translates a key system string into a CastKeySystem, calling into the
// platform for known key systems if needed.
CastKeySystem GetKeySystemByName(const std::string& key_system_name);

// Translates a platform-specific key system string into a CastKeySystem.
// TODO(gunsch): Remove when prefixed EME is removed.
CastKeySystem GetPlatformKeySystemByName(const std::string& key_system_name);

// Translates a platform-specific key system string into a CastKeySystem.
// TODO(gunsch): Remove when prefixed EME is removed.
#if defined(OS_ANDROID)
std::vector<::media::MediaClientAndroid::KeySystemUuidMap::value_type>
GetPlatformKeySystemUUIDMappings();
#endif

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_KEY_SYSTEMS_COMMON_H_
