// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_KEY_SYSTEMS_COMMON_H_
#define CHROMECAST_MEDIA_BASE_KEY_SYSTEMS_COMMON_H_

#include <string>

namespace chromecast {
namespace media {

extern const char kChromecastPlayreadyKeySystem[];

enum CastKeySystem {
  KEY_SYSTEM_NONE = 0,
  KEY_SYSTEM_CLEAR_KEY,
  KEY_SYSTEM_PLAYREADY,
  KEY_SYSTEM_WIDEVINE
};

// Translates a key system string into a CastKeySystem, calling into the
// platform for known key systems if needed.
CastKeySystem GetKeySystemByName(const std::string& key_system_name);

// Translates a platform-specific key system string into a CastKeySystem.
// TODO(gunsch): Remove when prefixed EME is removed.
CastKeySystem GetPlatformKeySystemByName(const std::string& key_system_name);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_KEY_SYSTEMS_COMMON_H_
