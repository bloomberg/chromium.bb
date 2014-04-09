// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "content/public/renderer/render_thread.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

// The following must be after widevine_cdm_version.h.

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#include <gnu/libc-version.h>
#include "base/version.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/common/encrypted_media_messages_android.h"
#endif

using content::KeySystemInfo;

const char kAudioWebM[] = "audio/webm";
const char kVideoWebM[] = "video/webm";
const char kVorbis[] = "vorbis";
const char kVorbisVP8[] = "vorbis,vp8,vp8.0";

#if defined(USE_PROPRIETARY_CODECS)
const char kAudioMp4[] = "audio/mp4";
const char kVideoMp4[] = "video/mp4";
const char kMp4a[] = "mp4a";
#if defined(WIDEVINE_CDM_AVAILABLE)
const char kAvc1Avc3[] = "avc1,avc3";
#endif  // WIDEVINE_CDM_AVAILABLE
const char kMp4aAvc1Avc3[] = "mp4a,avc1,avc3";
#endif  // defined(USE_PROPRIETARY_CODECS)

#if defined(ENABLE_PEPPER_CDMS)
static bool IsPepperCdmRegistered(
    const std::string& pepper_type,
    std::vector<base::string16>* additional_param_names,
    std::vector<base::string16>* additional_param_values) {
  bool is_registered = false;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_IsInternalPluginRegisteredForMimeType(
          pepper_type,
          &is_registered,
          additional_param_names,
          additional_param_values));

  return is_registered;
}

// External Clear Key (used for testing).
static void AddExternalClearKey(
    std::vector<KeySystemInfo>* concrete_key_systems) {
  static const char kExternalClearKeyKeySystem[] =
      "org.chromium.externalclearkey";
  static const char kExternalClearKeyDecryptOnlyKeySystem[] =
      "org.chromium.externalclearkey.decryptonly";
  static const char kExternalClearKeyFileIOTestKeySystem[] =
      "org.chromium.externalclearkey.fileiotest";
  static const char kExternalClearKeyInitializeFailKeySystem[] =
      "org.chromium.externalclearkey.initializefail";
  static const char kExternalClearKeyCrashKeySystem[] =
      "org.chromium.externalclearkey.crash";
  static const char kExternalClearKeyPepperType[] =
      "application/x-ppapi-clearkey-cdm";

  std::vector<base::string16> additional_param_names;
  std::vector<base::string16> additional_param_values;
  if (!IsPepperCdmRegistered(kExternalClearKeyPepperType,
                             &additional_param_names,
                             &additional_param_values)) {
    return;
  }

  KeySystemInfo info(kExternalClearKeyKeySystem);

  info.supported_types.push_back(std::make_pair(kAudioWebM, kVorbis));
  info.supported_types.push_back(std::make_pair(kVideoWebM, kVorbisVP8));
#if defined(USE_PROPRIETARY_CODECS)
  info.supported_types.push_back(std::make_pair(kAudioMp4, kMp4a));
  info.supported_types.push_back(std::make_pair(kVideoMp4, kMp4aAvc1Avc3));
#endif  // defined(USE_PROPRIETARY_CODECS)
  info.pepper_type = kExternalClearKeyPepperType;

  concrete_key_systems->push_back(info);

  // Add support of decrypt-only mode in ClearKeyCdm.
  info.key_system = kExternalClearKeyDecryptOnlyKeySystem;
  concrete_key_systems->push_back(info);

  // A key system that triggers FileIO test in ClearKeyCdm.
  info.key_system = kExternalClearKeyFileIOTestKeySystem;
  concrete_key_systems->push_back(info);

  // A key system that Chrome thinks is supported by ClearKeyCdm, but actually
  // will be refused by ClearKeyCdm. This is to test the CDM initialization
  // failure case.
  info.key_system = kExternalClearKeyInitializeFailKeySystem;
  concrete_key_systems->push_back(info);

  // A key system that triggers a crash in ClearKeyCdm.
  info.key_system = kExternalClearKeyCrashKeySystem;
  concrete_key_systems->push_back(info);
}
#endif  // defined(ENABLE_PEPPER_CDMS)


#if defined(WIDEVINE_CDM_AVAILABLE)
enum WidevineCdmType {
  WIDEVINE,
  WIDEVINE_HR,
#if defined(OS_ANDROID)
  WIDEVINE_HR_NON_COMPOSITING,
#endif
};

// Defines bitmask values used to specify supported codecs.
// Each value represents a codec within a specific container.
// The mask values are stored in a SupportedCodecs.
typedef uint32 SupportedCodecs;
enum SupportedCodecMasks {
  NO_CODECS = 0,
  WEBM_VP8_AND_VORBIS = 1 << 0,
#if defined(USE_PROPRIETARY_CODECS)
  MP4_AAC = 1 << 1,
  MP4_AVC1 = 1 << 2,
  MP4_CODECS = (MP4_AAC | MP4_AVC1),
#endif  // defined(USE_PROPRIETARY_CODECS)
};

#if defined(OS_ANDROID)
#define COMPILE_ASSERT_MATCHING_ENUM(name) \
  COMPILE_ASSERT(static_cast<int>(name) == \
                 static_cast<int>(android::name), \
                 mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(WEBM_VP8_AND_VORBIS);
COMPILE_ASSERT_MATCHING_ENUM(MP4_AAC);
COMPILE_ASSERT_MATCHING_ENUM(MP4_AVC1);
#undef COMPILE_ASSERT_MATCHING_ENUM
#else
static bool IsWidevineHrSupported() {
  // TODO(jrummell): Need to call CheckPlatformState() but it is
  // asynchronous, and needs to be done in the browser.
  return false;
}
#endif

// Return |name|'s parent key system.
static std::string GetDirectParentName(std::string name) {
  int last_period = name.find_last_of('.');
  DCHECK_GT(last_period, 0);
  return name.substr(0, last_period);
}

static void AddWidevineWithCodecs(
    WidevineCdmType widevine_cdm_type,
    SupportedCodecs supported_codecs,
    std::vector<KeySystemInfo>* concrete_key_systems) {

  KeySystemInfo info(kWidevineKeySystem);

  switch (widevine_cdm_type) {
    case WIDEVINE:
      // For standard Widevine, add parent name.
      info.parent_key_system = GetDirectParentName(kWidevineKeySystem);
      break;
    case WIDEVINE_HR:
      info.key_system.append(".hr");
      break;
#if defined(OS_ANDROID)
    case WIDEVINE_HR_NON_COMPOSITING:
      info.key_system.append(".hrnoncompositing");
      break;
#endif
    default:
      NOTREACHED();
  }

  if (supported_codecs & WEBM_VP8_AND_VORBIS) {
    info.supported_types.push_back(std::make_pair(kAudioWebM, kVorbis));
    info.supported_types.push_back(std::make_pair(kVideoWebM, kVorbisVP8));
  }

#if defined(USE_PROPRIETARY_CODECS)
  if (supported_codecs & MP4_CODECS) {
    // MP4 container is supported for audio and video if any codec is supported.
    bool is_aac_supported = (supported_codecs & MP4_AAC) != NO_CODECS;
    bool is_avc1_supported = (supported_codecs & MP4_AVC1) != NO_CODECS;
    const char* video_codecs = is_avc1_supported ?
                               (is_aac_supported ? kMp4aAvc1Avc3 : kAvc1Avc3) :
                               "";
    const char* audio_codecs = is_aac_supported ? kMp4a : "";

    info.supported_types.push_back(std::make_pair(kAudioMp4, audio_codecs));
    info.supported_types.push_back(std::make_pair(kVideoMp4, video_codecs));
  }
#endif  // defined(USE_PROPRIETARY_CODECS)

#if defined(ENABLE_PEPPER_CDMS)
  info.pepper_type = kWidevineCdmPluginMimeType;
#endif  // defined(ENABLE_PEPPER_CDMS)

  concrete_key_systems->push_back(info);
}

#if defined(ENABLE_PEPPER_CDMS)
// When the adapter is registered, a name-value pair is inserted in
// additional_param_* that lists the supported codecs. The name is "codecs" and
// the value is a comma-delimited list of codecs.
// This function finds "codecs" and parses the value into the vector |codecs|.
// Converts the codec strings to UTF-8 since we only expect ASCII strings and
// this simplifies the rest of the code in this file.
void GetSupportedCodecs(
    const std::vector<base::string16>& additional_param_names,
    const std::vector<base::string16>& additional_param_values,
    std::vector<std::string>* codecs) {
  DCHECK(codecs->empty());
  DCHECK_EQ(additional_param_names.size(), additional_param_values.size());
  for (size_t i = 0; i < additional_param_names.size(); ++i) {
    if (additional_param_names[i] ==
        base::ASCIIToUTF16(kCdmSupportedCodecsParamName)) {
      const base::string16& codecs_string16 = additional_param_values[i];
      std::string codecs_string;
      if (!base::UTF16ToUTF8(codecs_string16.c_str(),
                             codecs_string16.length(),
                             &codecs_string)) {
        DLOG(WARNING) << "Non-UTF-8 codecs string.";
        // Continue using the best effort conversion.
      }
      base::SplitString(codecs_string,
                        kCdmSupportedCodecsValueDelimiter,
                        codecs);
      break;
    }
  }
}

static void AddPepperBasedWidevine(
    std::vector<KeySystemInfo>* concrete_key_systems) {
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
  Version glibc_version(gnu_get_libc_version());
  DCHECK(glibc_version.IsValid());
  if (glibc_version.IsOlderThan(WIDEVINE_CDM_MIN_GLIBC_VERSION))
    return;
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)

  std::vector<base::string16> additional_param_names;
  std::vector<base::string16> additional_param_values;
  if (!IsPepperCdmRegistered(kWidevineCdmPluginMimeType,
                             &additional_param_names,
                             &additional_param_values)) {
    DVLOG(1) << "Widevine CDM is not currently available.";
    return;
  }

  std::vector<std::string> codecs;
  GetSupportedCodecs(additional_param_names, additional_param_values, &codecs);

  SupportedCodecs supported_codecs = NO_CODECS;
  for (size_t i = 0; i < codecs.size(); ++i) {
    // TODO(ddorwin): Break up VP8 and Vorbis. For now, "vp8" implies both.
    if (codecs[i] == kCdmSupportedCodecVp8)
      supported_codecs |= WEBM_VP8_AND_VORBIS;
#if defined(USE_PROPRIETARY_CODECS)
    if (codecs[i] == kCdmSupportedCodecAac)
      supported_codecs |= MP4_AAC;
    if (codecs[i] == kCdmSupportedCodecAvc1)
      supported_codecs |= MP4_AVC1;
#endif  // defined(USE_PROPRIETARY_CODECS)
  }

  AddWidevineWithCodecs(WIDEVINE, supported_codecs, concrete_key_systems);

  if (IsWidevineHrSupported())
    AddWidevineWithCodecs(WIDEVINE_HR, supported_codecs, concrete_key_systems);
}
#elif defined(OS_ANDROID)
static void AddAndroidWidevine(
    std::vector<KeySystemInfo>* concrete_key_systems) {
  SupportedKeySystemRequest request;
  SupportedKeySystemResponse response;

  request.key_system = kWidevineKeySystem;
  request.codecs = static_cast<android::SupportedCodecs>(
      android::WEBM_VP8_AND_VORBIS | android::MP4_AAC | android::MP4_AVC1);
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_GetSupportedKeySystems(request, &response));
  DCHECK_EQ(response.compositing_codecs >> 3, 0) << "unrecognized codec";
  DCHECK_EQ(response.non_compositing_codecs >> 3, 0) << "unrecognized codec";
  if (response.compositing_codecs != android::NO_SUPPORTED_CODECS) {
    AddWidevineWithCodecs(
        WIDEVINE,
        static_cast<SupportedCodecs>(response.compositing_codecs),
        concrete_key_systems);
  }

  if (response.non_compositing_codecs != android::NO_SUPPORTED_CODECS) {
    AddWidevineWithCodecs(
        WIDEVINE_HR_NON_COMPOSITING,
        static_cast<SupportedCodecs>(response.non_compositing_codecs),
        concrete_key_systems);
  }
}
#endif  // defined(ENABLE_PEPPER_CDMS)
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

void AddChromeKeySystems(std::vector<KeySystemInfo>* key_systems_info) {
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
