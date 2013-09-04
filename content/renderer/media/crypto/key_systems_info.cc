// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/key_systems_info.h"

#include "base/logging.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "third_party/WebKit/public/platform/WebString.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

// The following must be after widevine_cdm_version.h.

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#include <gnu/libc-version.h>
#include "base/version.h"
#endif

#if defined(DISABLE_WIDEVINE_CDM_CANPLAYTYPE)
#include "base/command_line.h"
#include "media/base/media_switches.h"
#endif

namespace content {

static const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";

#if defined(ENABLE_PEPPER_CDMS)
static const char kExternalClearKeyKeySystem[] =
    "org.chromium.externalclearkey";
#elif defined(OS_ANDROID)
static const uint8 kEmptyUuid[16] =
    { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
      0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
static const uint8 kWidevineUuid[16] =
    { 0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
      0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED };
#endif

#if defined(WIDEVINE_CDM_AVAILABLE)

#if defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)
// The supported codecs depend on what the CDM provides.
static const char kWidevineVideoMp4Codecs[] =
#if defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE) && \
    defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
    "avc1,mp4a";
#elif defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE)
    "avc1";
#else
    "";  // No codec strings are supported.
#endif

static const char kWidevineAudioMp4Codecs[] =
#if defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
    "mp4a";
#else
    "";  // No codec strings are supported.
#endif
#endif  // defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)

static void RegisterWidevine() {
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
  Version glibc_version(gnu_get_libc_version());
  DCHECK(glibc_version.IsValid());
  if (glibc_version.IsOlderThan(WIDEVINE_CDM_MIN_GLIBC_VERSION))
    return;
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)

  AddConcreteSupportedKeySystem(
      kWidevineKeySystem,
      false,
#if defined(ENABLE_PEPPER_CDMS)
      kWidevineCdmPluginMimeType,
#elif defined(OS_ANDROID)
      kWidevineUuid,
#endif  // defined(ENABLE_PEPPER_CDMS)
      "com.widevine");
  AddSupportedType(kWidevineKeySystem, "video/webm", "vorbis,vp8,vp8.0");
  AddSupportedType(kWidevineKeySystem, "audio/webm", "vorbis");
#if defined(USE_PROPRIETARY_CODECS) && \
    defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)
  AddSupportedType(kWidevineKeySystem, "video/mp4", kWidevineVideoMp4Codecs);
  AddSupportedType(kWidevineKeySystem, "audio/mp4", kWidevineAudioMp4Codecs);
#endif  // defined(USE_PROPRIETARY_CODECS) &&
        // defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)
}
#endif  // WIDEVINE_CDM_AVAILABLE


static void RegisterClearKey() {
  // Clear Key.
  AddConcreteSupportedKeySystem(
      kClearKeyKeySystem,
      true,
#if defined(ENABLE_PEPPER_CDMS)
      std::string(),
#elif defined(OS_ANDROID)
      kEmptyUuid,
#endif  // defined(ENABLE_PEPPER_CDMS)
      std::string());
  AddSupportedType(kClearKeyKeySystem, "video/webm", "vorbis,vp8,vp8.0");
  AddSupportedType(kClearKeyKeySystem, "audio/webm", "vorbis");
#if defined(USE_PROPRIETARY_CODECS)
  AddSupportedType(kClearKeyKeySystem, "video/mp4", "avc1,mp4a");
  AddSupportedType(kClearKeyKeySystem, "audio/mp4", "mp4a");
#endif  // defined(USE_PROPRIETARY_CODECS)
}

#if defined(ENABLE_PEPPER_CDMS)
static void RegisterExternalClearKey() {
  // External Clear Key (used for testing).
  AddConcreteSupportedKeySystem(kExternalClearKeyKeySystem, false,
                                "application/x-ppapi-clearkey-cdm",
                                std::string());
  AddSupportedType(kExternalClearKeyKeySystem,
                   "video/webm", "vorbis,vp8,vp8.0");
  AddSupportedType(kExternalClearKeyKeySystem, "audio/webm", "vorbis");
#if defined(USE_PROPRIETARY_CODECS)
  AddSupportedType(kExternalClearKeyKeySystem, "video/mp4", "avc1,mp4a");
  AddSupportedType(kExternalClearKeyKeySystem, "audio/mp4", "mp4a");
#endif  // defined(USE_PROPRIETARY_CODECS)
}
#endif  // defined(ENABLE_PEPPER_CDMS)

void RegisterKeySystems() {
  RegisterClearKey();

#if defined(ENABLE_PEPPER_CDMS)
  RegisterExternalClearKey();
#endif

#if defined(WIDEVINE_CDM_AVAILABLE)
  RegisterWidevine();
#endif
}

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
