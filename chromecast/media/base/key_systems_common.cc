// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/key_systems_common.h"

#include <cstddef>

#include "media/cdm/key_system_names.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

namespace chromecast {
namespace media {

const char kChromecastPlayreadyKeySystem[] = "com.chromecast.playready";

CastKeySystem GetKeySystemByName(const std::string& key_system_name) {
#if defined(WIDEVINE_CDM_AVAILABLE)
  if (key_system_name.compare(kWidevineKeySystem) == 0) {
    return KEY_SYSTEM_WIDEVINE;
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

#if defined(PLAYREADY_CDM_AVAILABLE)
  if (key_system_name.compare(kChromecastPlayreadyKeySystem) == 0) {
    return KEY_SYSTEM_PLAYREADY;
  }
#endif  // defined(PLAYREADY_CDM_AVAILABLE)

  if (key_system_name.compare(::media::kClearKey) == 0) {
    return KEY_SYSTEM_CLEAR_KEY;
  }

  return GetPlatformKeySystemByName(key_system_name);
}

}  // namespace media
}  // namespace chromecast
