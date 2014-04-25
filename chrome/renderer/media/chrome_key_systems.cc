// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems.h"

#include <string>
#include <vector>

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
using content::SupportedCodecs;

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

  info.supported_codecs = content::EME_CODEC_WEBM_ALL;
#if defined(USE_PROPRIETARY_CODECS)
  info.supported_codecs |= content::EME_CODEC_MP4_ALL;
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

#if !defined(OS_ANDROID)
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

  // TODO(xhwang): A container or an initDataType may be supported even though
  // there are no codecs supported in that container. Fix this when we support
  // initDataType.
  info.supported_codecs = supported_codecs;

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

  SupportedCodecs supported_codecs = content::EME_CODEC_NONE;
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (codecs[i] == kCdmSupportedCodecVorbis)
      supported_codecs |= content::EME_CODEC_WEBM_VORBIS;
    if (codecs[i] == kCdmSupportedCodecVp8)
      supported_codecs |= content::EME_CODEC_WEBM_VP8;
#if defined(USE_PROPRIETARY_CODECS)
    if (codecs[i] == kCdmSupportedCodecAac)
      supported_codecs |= content::EME_CODEC_MP4_AAC;
    if (codecs[i] == kCdmSupportedCodecAvc1)
      supported_codecs |= content::EME_CODEC_MP4_AVC1;
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
  request.codecs = content::EME_CODEC_WEBM_ALL | content::EME_CODEC_MP4_ALL;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_GetSupportedKeySystems(request, &response));
  DCHECK(response.compositing_codecs & content::EME_CODEC_ALL)
      << "unrecognized codec";
  DCHECK(response.non_compositing_codecs & content::EME_CODEC_ALL)
      << "unrecognized codec";
  if (response.compositing_codecs != content::EME_CODEC_NONE) {
    AddWidevineWithCodecs(
        WIDEVINE,
        static_cast<SupportedCodecs>(response.compositing_codecs),
        concrete_key_systems);
  }

  if (response.non_compositing_codecs != content::EME_CODEC_NONE) {
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
