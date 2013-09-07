// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/key_systems_info.h"

#include "base/logging.h"
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

static const char kAudioWebM[] = "audio/webm";
static const char kVideoWebM[] = "video/webm";
static const char kVorbis[] = "vorbis";
static const char kVorbisVP8[] = "vorbis,vp8,vp8.0";

static const char kAudioMp4[] = "audio/mp4";
static const char kVideoMp4[] = "video/mp4";
static const char kMp4a[] = "mp4a";
static const char kAvc1[] = "avc1";
static const char kMp4aAvc1[] = "mp4a,avc1";

#if defined(WIDEVINE_CDM_AVAILABLE)
enum SupportedCodecs {
  WEBM_VP8_AND_VORBIS = 1 << 0,
#if defined(USE_PROPRIETARY_CODECS)
  MP4_AAC = 1 << 1,
  MP4_AVC1 = 1 << 2,
#endif  // defined(USE_PROPRIETARY_CODECS)
};

static void AddWidevineForTypes(
    SupportedCodecs supported_codecs,
    std::vector<KeySystemInfo>* concrete_key_systems) {
  static const char kWidevineParentKeySystem[] = "com.widevine";
#if defined(OS_ANDROID)
  static const uint8 kWidevineUuid[16] = {
      0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
      0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED };
#endif

#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
  Version glibc_version(gnu_get_libc_version());
  DCHECK(glibc_version.IsValid());
  if (glibc_version.IsOlderThan(WIDEVINE_CDM_MIN_GLIBC_VERSION))
    return;
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)

  KeySystemInfo info(kWidevineKeySystem);

  if (supported_codecs & WEBM_VP8_AND_VORBIS) {
    info.supported_types.push_back(std::make_pair(kAudioWebM, kVorbis));
    info.supported_types.push_back(std::make_pair(kVideoWebM, kVorbisVP8));
  }

#if defined(USE_PROPRIETARY_CODECS)
  if (supported_codecs & MP4_AAC)
    info.supported_types.push_back(std::make_pair(kAudioMp4, kMp4a));

  if (supported_codecs & MP4_AVC1) {
    const char* video_codecs = (supported_codecs & MP4_AAC) ? kMp4aAvc1 : kAvc1;
    info.supported_types.push_back(std::make_pair(kVideoMp4, video_codecs));
  }
#endif  // defined(USE_PROPRIETARY_CODECS)

  info.parent_key_system = kWidevineParentKeySystem;

#if defined(ENABLE_PEPPER_CDMS)
  info.pepper_type = kWidevineCdmPluginMimeType;
#elif defined(OS_ANDROID)
  info.uuid.assign(kWidevineUuid, kWidevineUuid + arraysize(kWidevineUuid));
#endif  // defined(ENABLE_PEPPER_CDMS)

  concrete_key_systems->push_back(info);
}

#if defined(ENABLE_PEPPER_CDMS)
// Supported types are determined at compile time.
static void AddPepperBasedWidevine(
    std::vector<KeySystemInfo>* concrete_key_systems) {
  SupportedCodecs supported_codecs = WEBM_VP8_AND_VORBIS;

#if defined(USE_PROPRIETARY_CODECS)
#if defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
  supported_codecs |= MP4_AAC;
#endif
#if defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE)
  supported_codecs |= MP4_AVC1;
#endif
#endif  // defined(USE_PROPRIETARY_CODECS)

  AddWidevineForTypes(supported_codecs, concrete_key_systems);
}
#elif defined(OS_ANDROID)
static void AddAndroidWidevine(
    std::vector<KeySystemInfo>* concrete_key_systems) {
  SupportedCodecs supported_codecs = MP4_AAC | MP4_AVC1;
  AddWidevineForTypes(supported_codecs, concrete_key_systems);
}
#endif  // defined(ENABLE_PEPPER_CDMS)
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

static void AddClearKey(std::vector<KeySystemInfo>* concrete_key_systems) {
  KeySystemInfo info(kClearKeyKeySystem);

  info.supported_types.push_back(std::make_pair(kAudioWebM, kVorbis));
  info.supported_types.push_back(std::make_pair(kVideoWebM, kVorbisVP8));
#if defined(USE_PROPRIETARY_CODECS)
  info.supported_types.push_back(std::make_pair(kAudioMp4, kMp4a));
  info.supported_types.push_back(std::make_pair(kVideoMp4, kMp4aAvc1));
#endif  // defined(USE_PROPRIETARY_CODECS)

  info.use_aes_decryptor = true;

  concrete_key_systems->push_back(info);
}

#if defined(ENABLE_PEPPER_CDMS)
// External Clear Key (used for testing).
static void AddExternalClearKey(
    std::vector<KeySystemInfo>* concrete_key_systems) {
  static const char kExternalClearKeyKeySystem[] =
      "org.chromium.externalclearkey";
  static const char kExternalClearKeyPepperType[] =
      "application/x-ppapi-clearkey-cdm";

  KeySystemInfo info(kExternalClearKeyKeySystem);

  info.supported_types.push_back(std::make_pair(kAudioWebM, kVorbis));
  info.supported_types.push_back(std::make_pair(kVideoWebM, kVorbisVP8));
#if defined(USE_PROPRIETARY_CODECS)
  info.supported_types.push_back(std::make_pair(kAudioMp4, kMp4a));
  info.supported_types.push_back(std::make_pair(kVideoMp4, kMp4aAvc1));
#endif  // defined(USE_PROPRIETARY_CODECS)

  info.pepper_type = kExternalClearKeyPepperType;

  concrete_key_systems->push_back(info);
}
#endif  // defined(ENABLE_PEPPER_CDMS)

void AddKeySystems(std::vector<KeySystemInfo>* key_systems_info) {
  AddClearKey(key_systems_info);

#if defined(ENABLE_PEPPER_CDMS)
  AddExternalClearKey(key_systems_info);
#endif

#if defined(WIDEVINE_CDM_AVAILABLE)
#if defined(ENABLE_PEPPER_CDMS)
  AddPepperBasedWidevine(key_systems_info);
#elif defined(OS_ANDROID)
  AddAndroidWidevine(key_systems_info);
#endif
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
