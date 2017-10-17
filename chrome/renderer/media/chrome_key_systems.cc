// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/render_messages.h"
#include "components/cdm/renderer/external_clear_key_key_system_properties.h"
#include "components/cdm/renderer/widevine_key_system_properties.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_properties.h"
#include "media/media_features.h"

#if defined(OS_ANDROID)
#include "components/cdm/renderer/android_key_systems.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "base/feature_list.h"
#include "media/base/media_switches.h"
#endif

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

// The following must be after widevine_cdm_version.h.

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
#include <gnu/libc-version.h>
#include "base/version.h"
#endif

using media::EmeFeatureSupport;
using media::EmeSessionTypeSupport;
using media::KeySystemProperties;
using media::SupportedCodecs;

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
static bool IsPepperCdmAvailable(
    const std::string& pepper_type,
    std::vector<base::string16>* additional_param_names,
    std::vector<base::string16>* additional_param_values) {
  bool is_available = false;
  content::RenderThread::Get()->Send(
      new ChromeViewHostMsg_IsInternalPluginAvailableForMimeType(
          pepper_type,
          &is_available,
          additional_param_names,
          additional_param_values));

  return is_available;
}

// External Clear Key (used for testing).
static void AddExternalClearKey(
    std::vector<std::unique_ptr<KeySystemProperties>>* concrete_key_systems) {
  // TODO(xhwang): Move these into an array so we can use a for loop to add
  // supported key systems below.
  static const char kExternalClearKeyKeySystem[] =
      "org.chromium.externalclearkey";
  static const char kExternalClearKeyDecryptOnlyKeySystem[] =
      "org.chromium.externalclearkey.decryptonly";
  static const char kExternalClearKeyRenewalKeySystem[] =
      "org.chromium.externalclearkey.renewal";
  static const char kExternalClearKeyFileIOTestKeySystem[] =
      "org.chromium.externalclearkey.fileiotest";
  static const char kExternalClearKeyOutputProtectionTestKeySystem[] =
      "org.chromium.externalclearkey.outputprotectiontest";
  static const char kExternalClearKeyPlatformVerificationTestKeySystem[] =
      "org.chromium.externalclearkey.platformverificationtest";
  static const char kExternalClearKeyInitializeFailKeySystem[] =
      "org.chromium.externalclearkey.initializefail";
  static const char kExternalClearKeyCrashKeySystem[] =
      "org.chromium.externalclearkey.crash";
  static const char kExternalClearKeyVerifyCdmHostTestKeySystem[] =
      "org.chromium.externalclearkey.verifycdmhosttest";
  static const char kExternalClearKeyStorageIdTestKeySystem[] =
      "org.chromium.externalclearkey.storageidtest";
  static const char kExternalClearKeyDifferentGuidTestKeySystem[] =
      "org.chromium.externalclearkey.differentguid";

  std::vector<base::string16> additional_param_names;
  std::vector<base::string16> additional_param_values;
  if (!IsPepperCdmAvailable(cdm::kExternalClearKeyPepperType,
                            &additional_param_names,
                            &additional_param_values)) {
    return;
  }

  concrete_key_systems->emplace_back(
      new cdm::ExternalClearKeyProperties(kExternalClearKeyKeySystem));

  // Add support of decrypt-only mode in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyDecryptOnlyKeySystem));

  // A key system that triggers renewal message in ClearKeyCdm.
  concrete_key_systems->emplace_back(
      new cdm::ExternalClearKeyProperties(kExternalClearKeyRenewalKeySystem));

  // A key system that triggers the FileIO test in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyFileIOTestKeySystem));

  // A key system that triggers the output protection test in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyOutputProtectionTestKeySystem));

  // A key system that triggers the platform verification test in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyPlatformVerificationTestKeySystem));

  // A key system that Chrome thinks is supported by ClearKeyCdm, but actually
  // will be refused by ClearKeyCdm. This is to test the CDM initialization
  // failure case.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyInitializeFailKeySystem));

  // A key system that triggers a crash in ClearKeyCdm.
  concrete_key_systems->emplace_back(
      new cdm::ExternalClearKeyProperties(kExternalClearKeyCrashKeySystem));

  // A key system that triggers the verify host files test in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyVerifyCdmHostTestKeySystem));

  // A key system that fetches the Storage ID in ClearKeyCdm.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyStorageIdTestKeySystem));

  // A key system that is registered with a different CDM GUID.
  concrete_key_systems->emplace_back(new cdm::ExternalClearKeyProperties(
      kExternalClearKeyDifferentGuidTestKeySystem));
}

#if defined(WIDEVINE_CDM_AVAILABLE)
// This function finds "codecs" and parses the value into the vector |codecs|.
// Converts the codec strings to UTF-8 since we only expect ASCII strings and
// this simplifies the rest of the code in this file.
void GetSupportedCodecsForPepperCdm(
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
      *codecs = base::SplitString(
          codecs_string, std::string(1, kCdmSupportedCodecsValueDelimiter),
          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      break;
    }
  }
}

// Whether persistent-license session is supported by the CDM.
bool IsPersistentLicenseSupportedbyCdm(
    const std::vector<base::string16>& additional_param_names,
    const std::vector<base::string16>& additional_param_values) {
  DCHECK_EQ(additional_param_names.size(), additional_param_values.size());
  const base::string16 expected_param_name =
      base::ASCIIToUTF16(kCdmPersistentLicenseSupportedParamName);
  for (size_t i = 0; i < additional_param_names.size(); ++i) {
    if (additional_param_names[i] == expected_param_name) {
      return additional_param_values[i] ==
             base::ASCIIToUTF16(kCdmFeatureSupported);
    }
  }

  return false;
}

// Returns persistent-license session support.
EmeSessionTypeSupport GetPersistentLicenseSupport(bool supported_by_the_cdm) {
  if (!supported_by_the_cdm)
    return EmeSessionTypeSupport::NOT_SUPPORTED;

// On ChromeOS, platform verification is similar to CDM host verification.
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION) || defined(OS_CHROMEOS)
  bool cdm_host_verification_potentially_supported = true;
#else
  bool cdm_host_verification_potentially_supported = false;
#endif

  // If we are sure CDM host verification is NOT supported, we should not
  // support persistent-license.
  if (!cdm_host_verification_potentially_supported)
    return EmeSessionTypeSupport::NOT_SUPPORTED;

#if defined(OS_CHROMEOS)
  // On ChromeOS, platform verification (similar to CDM host verification)
  // requires identifier to be allowed.
  // TODO(jrummell): Currently the ChromeOS CDM does not require storage ID
  // to support persistent license. Update this logic when the new CDM requires
  // storage ID.
  return EmeSessionTypeSupport::SUPPORTED_WITH_IDENTIFIER;
#else
  // TODO(jrummell): Update this to reflect what's implemented.
  bool cdm_storage_id_supported = false;

  // On other platforms, we require storage ID to support persistent license.
  return cdm_storage_id_supported ? EmeSessionTypeSupport::SUPPORTED
                                  : EmeSessionTypeSupport::NOT_SUPPORTED;
#endif  // defined(OS_CHROMEOS)
}

static void AddPepperBasedWidevine(
    std::vector<std::unique_ptr<KeySystemProperties>>* concrete_key_systems) {
#if defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)
  base::Version glibc_version(gnu_get_libc_version());
  DCHECK(glibc_version.IsValid());
  if (glibc_version < base::Version(WIDEVINE_CDM_MIN_GLIBC_VERSION))
    return;
#endif  // defined(WIDEVINE_CDM_MIN_GLIBC_VERSION)

  std::vector<base::string16> additional_param_names;
  std::vector<base::string16> additional_param_values;
  if (!IsPepperCdmAvailable(kWidevineCdmPluginMimeType,
                            &additional_param_names,
                            &additional_param_values)) {
    DVLOG(1) << "Widevine CDM is not currently available.";
    return;
  }

  std::vector<std::string> codecs;
  GetSupportedCodecsForPepperCdm(additional_param_names,
                                 additional_param_values,
                                 &codecs);

  SupportedCodecs supported_codecs = media::EME_CODEC_NONE;

  // Audio codecs are always supported.
  // TODO(sandersd): Distinguish these from those that are directly supported,
  // as those may offer a higher level of protection.
  supported_codecs |= media::EME_CODEC_WEBM_OPUS;
  supported_codecs |= media::EME_CODEC_WEBM_VORBIS;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  supported_codecs |= media::EME_CODEC_MP4_AAC;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  for (size_t i = 0; i < codecs.size(); ++i) {
    if (codecs[i] == kCdmSupportedCodecVp8)
      supported_codecs |= media::EME_CODEC_WEBM_VP8;
    if (codecs[i] == kCdmSupportedCodecVp9) {
      supported_codecs |= media::EME_CODEC_WEBM_VP9;
      supported_codecs |= media::EME_CODEC_COMMON_VP9;
    }
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    if (codecs[i] == kCdmSupportedCodecAvc1)
      supported_codecs |= media::EME_CODEC_MP4_AVC1;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  }

  EmeSessionTypeSupport persistent_license_support =
      GetPersistentLicenseSupport(IsPersistentLicenseSupportedbyCdm(
          additional_param_names, additional_param_values));

  using Robustness = cdm::WidevineKeySystemProperties::Robustness;

  concrete_key_systems->emplace_back(new cdm::WidevineKeySystemProperties(
      supported_codecs,
#if defined(OS_CHROMEOS)
      Robustness::HW_SECURE_ALL,             // Maximum audio robustness.
      Robustness::HW_SECURE_ALL,             // Maximum video robustness.
      persistent_license_support,            // Persistent-license.
      EmeSessionTypeSupport::NOT_SUPPORTED,  // Persistent-release-message.
      EmeFeatureSupport::REQUESTABLE,        // Persistent state.
      EmeFeatureSupport::REQUESTABLE));      // Distinctive identifier.
#else                                        // Desktop
      Robustness::SW_SECURE_CRYPTO,          // Maximum audio robustness.
      Robustness::SW_SECURE_DECODE,          // Maximum video robustness.
      persistent_license_support,            // persistent-license.
      EmeSessionTypeSupport::NOT_SUPPORTED,  // persistent-release-message.
      EmeFeatureSupport::REQUESTABLE,        // Persistent state.
      EmeFeatureSupport::NOT_SUPPORTED));    // Distinctive identifier.
#endif                                       // defined(OS_CHROMEOS)
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

void AddChromeKeySystems(
    std::vector<std::unique_ptr<KeySystemProperties>>* key_systems_properties) {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if (base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting))
    AddExternalClearKey(key_systems_properties);

#if defined(WIDEVINE_CDM_AVAILABLE)
  AddPepperBasedWidevine(key_systems_properties);
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_ANDROID)
  cdm::AddAndroidWidevine(key_systems_properties);
#endif  // defined(OS_ANDROID)
}
