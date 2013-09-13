// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/key_systems_info.h"

#include "third_party/WebKit/public/platform/WebString.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

// The following must be after widevine_cdm_version.h.

#if defined(DISABLE_WIDEVINE_CDM_CANPLAYTYPE)
#include "base/command_line.h"
#include "media/base/media_switches.h"
#endif

namespace content {

static const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";

bool IsCanPlayTypeSuppressed(const std::string& key_system) {
#if defined(DISABLE_WIDEVINE_CDM_CANPLAYTYPE)
  // See http://crbug.com/237627.
  if (key_system == kWidevineKeySystem &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOverrideEncryptedMediaCanPlayType))
    return true;
#endif
  return false;
}

std::string KeySystemNameForUMAInternal(const WebKit::WebString& key_system) {
  if (key_system == kClearKeyKeySystem)
    return "ClearKey";
#if defined(WIDEVINE_CDM_AVAILABLE)
  if (key_system == kWidevineKeySystem)
    return "Widevine";
#endif  // WIDEVINE_CDM_AVAILABLE
  return "Unknown";
}

} // namespace content
