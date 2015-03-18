// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_systems.h"

#include <string>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_info.h"
#include "media/base/key_systems_support_uma.h"
#include "media/base/media_client.h"
#include "media/cdm/key_system_names.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace media {

const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kPrefixedClearKeyKeySystem[] = "webkit-org.w3.clearkey";
const char kUnsupportedClearKeyKeySystem[] = "unsupported-org.w3.clearkey";

// These names are used by UMA. Do not change them!
const char kClearKeyKeySystemNameForUMA[] = "ClearKey";
const char kUnknownKeySystemNameForUMA[] = "Unknown";

struct NamedInitDataType {
  const char* name;
  EmeInitDataType type;
};

// Mapping between initialization data types names and enum values. When adding
// entries, make sure to update IsSaneInitDataTypeWithContainer().
static NamedInitDataType kInitDataTypeNames[] = {
    {"webm", EME_INIT_DATA_TYPE_WEBM},
#if defined(USE_PROPRIETARY_CODECS)
    {"cenc", EME_INIT_DATA_TYPE_CENC},
#endif  // defined(USE_PROPRIETARY_CODECS)
    {"keyids", EME_INIT_DATA_TYPE_KEYIDS},
};

struct NamedCodec {
  const char* name;
  EmeCodec type;
};

// Mapping between containers and their codecs.
// Only audio codec can belong to a "audio/*" container. Both audio and video
// codecs can belong to a "video/*" container.
static NamedCodec kContainerToCodecMasks[] = {
    {"audio/webm", EME_CODEC_WEBM_AUDIO_ALL},
    {"video/webm", EME_CODEC_WEBM_ALL},
#if defined(USE_PROPRIETARY_CODECS)
    {"audio/mp4", EME_CODEC_MP4_AUDIO_ALL},
    {"video/mp4", EME_CODEC_MP4_ALL}
#endif  // defined(USE_PROPRIETARY_CODECS)
};

// Mapping between codec names and enum values.
static NamedCodec kCodecStrings[] = {
    {"opus", EME_CODEC_WEBM_OPUS},
    {"vorbis", EME_CODEC_WEBM_VORBIS},
    {"vp8", EME_CODEC_WEBM_VP8},
    {"vp8.0", EME_CODEC_WEBM_VP8},
    {"vp9", EME_CODEC_WEBM_VP9},
    {"vp9.0", EME_CODEC_WEBM_VP9},
#if defined(USE_PROPRIETARY_CODECS)
    {"mp4a", EME_CODEC_MP4_AAC},
    {"avc1", EME_CODEC_MP4_AVC1},
    {"avc3", EME_CODEC_MP4_AVC1}
#endif  // defined(USE_PROPRIETARY_CODECS)
};

static void AddClearKey(std::vector<KeySystemInfo>* concrete_key_systems) {
  KeySystemInfo info;
  info.key_system = kClearKeyKeySystem;

  // On Android, Vorbis, VP8, AAC and AVC1 are supported in MediaCodec:
  // http://developer.android.com/guide/appendix/media-formats.html
  // VP9 support is device dependent.

  info.supported_init_data_types =
      EME_INIT_DATA_TYPE_WEBM | EME_INIT_DATA_TYPE_KEYIDS;
  info.supported_codecs = EME_CODEC_WEBM_ALL;

#if defined(OS_ANDROID)
  // Temporarily disable VP9 support for Android.
  // TODO(xhwang): Use mime_util.h to query VP9 support on Android.
  info.supported_codecs &= ~EME_CODEC_WEBM_VP9;

  // Opus is not supported on Android yet. http://crbug.com/318436.
  // TODO(sandersd): Check for platform support to set this bit.
  info.supported_codecs &= ~EME_CODEC_WEBM_OPUS;
#endif  // defined(OS_ANDROID)

#if defined(USE_PROPRIETARY_CODECS)
  info.supported_init_data_types |= EME_INIT_DATA_TYPE_CENC;
  info.supported_codecs |= EME_CODEC_MP4_ALL;
#endif  // defined(USE_PROPRIETARY_CODECS)

  info.persistent_license_support = EME_SESSION_TYPE_NOT_SUPPORTED;
  info.persistent_release_message_support = EME_SESSION_TYPE_NOT_SUPPORTED;
  info.persistent_state_support = EME_FEATURE_NOT_SUPPORTED;
  info.distinctive_identifier_support = EME_FEATURE_NOT_SUPPORTED;

  info.use_aes_decryptor = true;

  concrete_key_systems->push_back(info);
}

// Returns whether the |key_system| is known to Chromium and is thus likely to
// be implemented in an interoperable way.
// True is always returned for a |key_system| that begins with "x-".
//
// As with other web platform features, advertising support for a key system
// implies that it adheres to a defined and interoperable specification.
//
// To ensure interoperability, implementations of a specific |key_system| string
// must conform to a specification for that identifier that defines
// key system-specific behaviors not fully defined by the EME specification.
// That specification should be provided by the owner of the domain that is the
// reverse of the |key_system| string.
// This involves more than calling a library, SDK, or platform API. KeySystems
// must be populated appropriately, and there will likely be glue code to adapt
// to the API of the library, SDK, or platform API.
//
// Chromium mainline contains this data and glue code for specific key systems,
// which should help ensure interoperability with other implementations using
// these key systems.
//
// If you need to add support for other key systems, ensure that you have
// obtained the specification for how to integrate it with EME, implemented the
// appropriate glue/adapter code, and added all the appropriate data to
// KeySystems. Only then should you change this function.
static bool IsPotentiallySupportedKeySystem(const std::string& key_system) {
  // Known and supported key systems.
  if (key_system == kWidevineKeySystem)
    return true;
  if (key_system == kClearKey)
    return true;

  // External Clear Key is known and supports suffixes for testing.
  if (IsExternalClearKey(key_system))
    return true;

  // Chromecast defines behaviors for Cast clients within its reverse domain.
  const char kChromecastRoot[] = "com.chromecast";
  if (IsParentKeySystemOf(kChromecastRoot, key_system))
    return true;

  // Implementations that do not have a specification or appropriate glue code
  // can use the "x-" prefix to avoid conflicting with and advertising support
  // for real key system names. Use is discouraged.
  const char kExcludedPrefix[] = "x-";
  if (key_system.find(kExcludedPrefix, 0, arraysize(kExcludedPrefix) - 1) == 0)
    return true;

  return false;
}

class KeySystems {
 public:
  static KeySystems& GetInstance();

  void UpdateIfNeeded();

  bool IsConcreteSupportedKeySystem(const std::string& key_system);

  bool IsSupportedKeySystem(const std::string& key_system);

  bool IsSupportedKeySystemWithInitDataType(
      const std::string& key_system,
      const std::string& init_data_type);

  bool IsSupportedKeySystemWithMediaMimeType(
      const std::string& mime_type,
      const std::vector<std::string>& codecs,
      const std::string& key_system,
      bool is_prefixed);

  std::string GetKeySystemNameForUMA(const std::string& key_system) const;

  bool UseAesDecryptor(const std::string& concrete_key_system);

#if defined(ENABLE_PEPPER_CDMS)
  std::string GetPepperType(const std::string& concrete_key_system);
#endif

  bool IsPersistentLicenseSessionSupported(
      const std::string& key_system,
      bool is_permission_granted);

  bool IsPersistentReleaseMessageSessionSupported(
      const std::string& key_system,
      bool is_permission_granted);

  bool IsPersistentStateRequirementSupported(
      const std::string& key_system,
      EmeFeatureRequirement requirement,
      bool is_permission_granted);

  bool IsDistinctiveIdentifierRequirementSupported(
      const std::string& key_system,
      EmeFeatureRequirement requirement,
      bool is_permission_granted);

  void AddContainerMask(const std::string& container, uint32 mask);
  void AddCodecMask(const std::string& codec, uint32 mask);

 private:
  void InitializeUMAInfo();

  void UpdateSupportedKeySystems();

  void AddConcreteSupportedKeySystems(
      const std::vector<KeySystemInfo>& concrete_key_systems);

  friend struct base::DefaultLazyInstanceTraits<KeySystems>;

  typedef base::hash_map<std::string, KeySystemInfo> KeySystemInfoMap;
  typedef base::hash_map<std::string, std::string> ParentKeySystemMap;
  typedef base::hash_map<std::string, SupportedCodecs> ContainerCodecsMap;
  typedef base::hash_map<std::string, EmeCodec> CodecsMap;
  typedef base::hash_map<std::string, EmeInitDataType> InitDataTypesMap;
  typedef base::hash_map<std::string, std::string> KeySystemNameForUMAMap;

  KeySystems();
  ~KeySystems() {}

  EmeInitDataType GetInitDataTypeForName(
      const std::string& init_data_type) const;
  // TODO(sandersd): Separate container enum from codec mask value.
  // http://crbug.com/417440
  SupportedCodecs GetCodecMaskForContainer(
      const std::string& container) const;
  EmeCodec GetCodecForString(const std::string& codec) const;

  const std::string& PrefixedGetConcreteKeySystemNameFor(
      const std::string& key_system) const;

  // Returns whether a |container| type is supported by checking
  // |key_system_supported_codecs|.
  // TODO(xhwang): Update this to actually check initDataType support.
  bool IsSupportedContainer(const std::string& container,
                            SupportedCodecs key_system_supported_codecs) const;

  // Returns true if all |codecs| are supported in |container| by checking
  // |key_system_supported_codecs|.
  bool IsSupportedContainerAndCodecs(
      const std::string& container,
      const std::vector<std::string>& codecs,
      SupportedCodecs key_system_supported_codecs) const;

  // Map from key system string to capabilities.
  KeySystemInfoMap concrete_key_system_map_;

  // Map from parent key system to the concrete key system that should be used
  // to represent its capabilities.
  ParentKeySystemMap parent_key_system_map_;

  KeySystemsSupportUMA key_systems_support_uma_;

  InitDataTypesMap init_data_type_name_map_;
  ContainerCodecsMap container_to_codec_mask_map_;
  CodecsMap codec_string_map_;
  KeySystemNameForUMAMap key_system_name_for_uma_map_;

  // Makes sure all methods are called from the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(KeySystems);
};

static base::LazyInstance<KeySystems> g_key_systems = LAZY_INSTANCE_INITIALIZER;

KeySystems& KeySystems::GetInstance() {
  KeySystems& key_systems = g_key_systems.Get();
  key_systems.UpdateIfNeeded();
  return key_systems;
}

// Because we use a LazyInstance, the key systems info must be populated when
// the instance is lazily initiated.
KeySystems::KeySystems() {
  for (size_t i = 0; i < arraysize(kInitDataTypeNames); ++i) {
    const std::string& name = kInitDataTypeNames[i].name;
    DCHECK(!init_data_type_name_map_.count(name));
    init_data_type_name_map_[name] = kInitDataTypeNames[i].type;
  }
  for (size_t i = 0; i < arraysize(kContainerToCodecMasks); ++i) {
    const std::string& name = kContainerToCodecMasks[i].name;
    DCHECK(!container_to_codec_mask_map_.count(name));
    container_to_codec_mask_map_[name] = kContainerToCodecMasks[i].type;
  }
  for (size_t i = 0; i < arraysize(kCodecStrings); ++i) {
    const std::string& name = kCodecStrings[i].name;
    DCHECK(!codec_string_map_.count(name));
    codec_string_map_[name] = kCodecStrings[i].type;
  }

  InitializeUMAInfo();

  // Always update supported key systems during construction.
  UpdateSupportedKeySystems();
}

EmeInitDataType KeySystems::GetInitDataTypeForName(
    const std::string& init_data_type) const {
  InitDataTypesMap::const_iterator iter =
      init_data_type_name_map_.find(init_data_type);
  if (iter != init_data_type_name_map_.end())
    return iter->second;
  return EME_INIT_DATA_TYPE_NONE;
}

SupportedCodecs KeySystems::GetCodecMaskForContainer(
    const std::string& container) const {
  ContainerCodecsMap::const_iterator iter =
      container_to_codec_mask_map_.find(container);
  if (iter != container_to_codec_mask_map_.end())
    return iter->second;
  return EME_CODEC_NONE;
}

EmeCodec KeySystems::GetCodecForString(const std::string& codec) const {
  CodecsMap::const_iterator iter = codec_string_map_.find(codec);
  if (iter != codec_string_map_.end())
    return iter->second;
  return EME_CODEC_NONE;
}

const std::string& KeySystems::PrefixedGetConcreteKeySystemNameFor(
    const std::string& key_system) const {
  ParentKeySystemMap::const_iterator iter =
      parent_key_system_map_.find(key_system);
  if (iter != parent_key_system_map_.end())
    return iter->second;
  return key_system;
}

void KeySystems::InitializeUMAInfo() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(key_system_name_for_uma_map_.empty());

  std::vector<KeySystemInfoForUMA> key_systems_info_for_uma;
  if (GetMediaClient())
    GetMediaClient()->AddKeySystemsInfoForUMA(&key_systems_info_for_uma);

  for (const KeySystemInfoForUMA& info : key_systems_info_for_uma) {
    key_system_name_for_uma_map_[info.key_system] =
        info.key_system_name_for_uma;
    if (info.reports_key_system_support_to_uma)
      key_systems_support_uma_.AddKeySystemToReport(info.key_system);
  }

  // Clear Key is always supported.
  key_system_name_for_uma_map_[kClearKeyKeySystem] =
      kClearKeyKeySystemNameForUMA;
}

void KeySystems::UpdateIfNeeded() {
  if (GetMediaClient() && GetMediaClient()->IsKeySystemsUpdateNeeded())
    UpdateSupportedKeySystems();
}

void KeySystems::UpdateSupportedKeySystems() {
  DCHECK(thread_checker_.CalledOnValidThread());
  concrete_key_system_map_.clear();
  parent_key_system_map_.clear();

  // Build KeySystemInfo.
  std::vector<KeySystemInfo> key_systems_info;

  // Add key systems supported by the MediaClient implementation.
  if (GetMediaClient())
    GetMediaClient()->AddSupportedKeySystems(&key_systems_info);

  // Clear Key is always supported.
  AddClearKey(&key_systems_info);

  AddConcreteSupportedKeySystems(key_systems_info);
}

void KeySystems::AddConcreteSupportedKeySystems(
    const std::vector<KeySystemInfo>& concrete_key_systems) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(concrete_key_system_map_.empty());
  DCHECK(parent_key_system_map_.empty());

  for (const KeySystemInfo& info : concrete_key_systems) {
    DCHECK(!info.key_system.empty());
    DCHECK_NE(info.persistent_license_support, EME_SESSION_TYPE_INVALID);
    DCHECK_NE(info.persistent_release_message_support,
              EME_SESSION_TYPE_INVALID);
    // TODO(sandersd): Add REQUESTABLE and REQUESTABLE_WITH_PERMISSION for
    // persistent_state_support once we can block access per-CDM-instance
    // (http://crbug.com/457482).
    DCHECK(info.persistent_state_support == EME_FEATURE_NOT_SUPPORTED ||
           info.persistent_state_support == EME_FEATURE_ALWAYS_ENABLED);
// TODO(sandersd): Allow REQUESTABLE_WITH_PERMISSION for all key systems on
// all platforms once we have proper enforcement (http://crbug.com/457482).
// On Chrome OS, an ID will not be used without permission, but we cannot
// currently prevent the CDM from requesting the permission again when no
// there was no initial prompt. Thus, we block "not-allowed" below.
#if defined(OS_CHROMEOS)
    DCHECK(info.distinctive_identifier_support == EME_FEATURE_NOT_SUPPORTED ||
           (info.distinctive_identifier_support ==
                EME_FEATURE_REQUESTABLE_WITH_PERMISSION &&
            info.key_system == kWidevineKeySystem) ||
           info.distinctive_identifier_support == EME_FEATURE_ALWAYS_ENABLED);
#else
    DCHECK(info.distinctive_identifier_support == EME_FEATURE_NOT_SUPPORTED ||
           info.distinctive_identifier_support == EME_FEATURE_ALWAYS_ENABLED);
#endif
    if (info.persistent_state_support == EME_FEATURE_NOT_SUPPORTED) {
      DCHECK_EQ(info.persistent_license_support,
                EME_SESSION_TYPE_NOT_SUPPORTED);
      DCHECK_EQ(info.persistent_release_message_support,
                EME_SESSION_TYPE_NOT_SUPPORTED);
    }
    DCHECK(!IsSupportedKeySystem(info.key_system))
        << "Key system '" << info.key_system << "' already registered";
    DCHECK(!parent_key_system_map_.count(info.key_system))
        <<  "'" << info.key_system << "' is already registered as a parent";
#if defined(ENABLE_PEPPER_CDMS)
    DCHECK_EQ(info.use_aes_decryptor, info.pepper_type.empty());
#endif

    concrete_key_system_map_[info.key_system] = info;

    if (!info.parent_key_system.empty()) {
      DCHECK(!IsConcreteSupportedKeySystem(info.parent_key_system))
          << "Parent '" << info.parent_key_system << "' "
          << "already registered concrete";
      DCHECK(!parent_key_system_map_.count(info.parent_key_system))
          << "Parent '" << info.parent_key_system << "' already registered";
      parent_key_system_map_[info.parent_key_system] = info.key_system;
    }
  }
}

bool KeySystems::IsConcreteSupportedKeySystem(const std::string& key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return concrete_key_system_map_.count(key_system) != 0;
}

bool KeySystems::IsSupportedContainer(
    const std::string& container,
    SupportedCodecs key_system_supported_codecs) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!container.empty());

  // When checking container support for EME, "audio/foo" should be treated the
  // same as "video/foo". Convert the |container| to achieve this.
  // TODO(xhwang): Replace this with real checks against supported initDataTypes
  // combined with supported demuxers.
  std::string canonical_container = container;
  if (container.find("audio/") == 0)
    canonical_container.replace(0, 6, "video/");

  // A container is supported iif at least one codec in that container is
  // supported.
  SupportedCodecs supported_codecs =
      GetCodecMaskForContainer(canonical_container);
  return (supported_codecs & key_system_supported_codecs) != 0;
}

bool KeySystems::IsSupportedContainerAndCodecs(
    const std::string& container,
    const std::vector<std::string>& codecs,
    SupportedCodecs key_system_supported_codecs) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!container.empty());
  DCHECK(!codecs.empty());
  DCHECK(IsSupportedContainer(container, key_system_supported_codecs));

  SupportedCodecs container_supported_codecs =
      GetCodecMaskForContainer(container);

  for (size_t i = 0; i < codecs.size(); ++i) {
    // TODO(sandersd): This should fail for isTypeSupported().
    // http://crbug.com/417461
    if (codecs[i].empty())
      continue;

    EmeCodec codec = GetCodecForString(codecs[i]);

    // Unsupported codec.
    if (!(codec & key_system_supported_codecs))
      return false;

    // Unsupported codec/container combination, e.g. "video/webm" and "avc1".
    if (!(codec & container_supported_codecs))
      return false;
  }

  return true;
}

bool KeySystems::IsSupportedKeySystem(const std::string& key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Unprefixed EME only supports concrete key systems.
  return concrete_key_system_map_.count(key_system) != 0;
}

bool KeySystems::IsSupportedKeySystemWithInitDataType(
    const std::string& key_system,
    const std::string& init_data_type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Locate |key_system|. Only concrete key systems are supported in unprefixed.
  KeySystemInfoMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(key_system);
  if (key_system_iter == concrete_key_system_map_.end())
    return false;

  // Check |init_data_type| and |key_system| x |init_data_type|.
  const KeySystemInfo& info = key_system_iter->second;
  EmeInitDataType eme_init_data_type = GetInitDataTypeForName(init_data_type);
  return (info.supported_init_data_types & eme_init_data_type) != 0;
}

// TODO(sandersd): Reorganize to be more similar to
// IsKeySystemSupportedWithInitDataType(). Note that a fork may still be
// required; http://crbug.com/417461.
bool KeySystems::IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system,
    bool is_prefixed) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const bool report_to_uma = is_prefixed;

  // If |is_prefixed| and |key_system| is a parent key system, use its concrete
  // child.
  const std::string& concrete_key_system = is_prefixed ?
      PrefixedGetConcreteKeySystemNameFor(key_system) :
      key_system;

  bool has_type = !mime_type.empty();

  if (report_to_uma)
    key_systems_support_uma_.ReportKeySystemQuery(key_system, has_type);

  // Check key system support.
  KeySystemInfoMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end())
    return false;

  if (report_to_uma)
    key_systems_support_uma_.ReportKeySystemSupport(key_system, false);

  if (!has_type) {
    DCHECK(codecs.empty());
    return true;
  }

  SupportedCodecs key_system_supported_codecs =
      key_system_iter->second.supported_codecs;

  if (!IsSupportedContainer(mime_type, key_system_supported_codecs))
    return false;

  if (!codecs.empty() &&
      !IsSupportedContainerAndCodecs(
          mime_type, codecs, key_system_supported_codecs)) {
    return false;
  }

  if (report_to_uma)
    key_systems_support_uma_.ReportKeySystemSupport(key_system, true);
  return true;
}

std::string KeySystems::GetKeySystemNameForUMA(
    const std::string& key_system) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  KeySystemNameForUMAMap::const_iterator iter =
      key_system_name_for_uma_map_.find(key_system);
  if (iter == key_system_name_for_uma_map_.end())
    return kUnknownKeySystemNameForUMA;

  return iter->second;
}

bool KeySystems::UseAesDecryptor(const std::string& concrete_key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  KeySystemInfoMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
      DLOG(FATAL) << concrete_key_system << " is not a known concrete system";
      return false;
  }

  return key_system_iter->second.use_aes_decryptor;
}

#if defined(ENABLE_PEPPER_CDMS)
std::string KeySystems::GetPepperType(const std::string& concrete_key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  KeySystemInfoMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
      DLOG(FATAL) << concrete_key_system << " is not a known concrete system";
      return std::string();
  }

  const std::string& type = key_system_iter->second.pepper_type;
  DLOG_IF(FATAL, type.empty()) << concrete_key_system << " is not Pepper-based";
  return type;
}
#endif

bool KeySystems::IsPersistentLicenseSessionSupported(
    const std::string& key_system,
    bool is_permission_granted) {
  DCHECK(thread_checker_.CalledOnValidThread());

  KeySystemInfoMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
    NOTREACHED();
    return false;
  }

  switch (key_system_iter->second.persistent_license_support) {
    case EME_SESSION_TYPE_INVALID:
      NOTREACHED();
      return false;
    case EME_SESSION_TYPE_NOT_SUPPORTED:
      return false;
    case EME_SESSION_TYPE_SUPPORTED_WITH_PERMISSION:
      return is_permission_granted;
    case EME_SESSION_TYPE_SUPPORTED:
      return true;
  }

  NOTREACHED();
  return false;
}

bool KeySystems::IsPersistentReleaseMessageSessionSupported(
    const std::string& key_system,
    bool is_permission_granted) {
  DCHECK(thread_checker_.CalledOnValidThread());

  KeySystemInfoMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
    NOTREACHED();
    return false;
  }

  switch (key_system_iter->second.persistent_release_message_support) {
    case EME_SESSION_TYPE_INVALID:
      NOTREACHED();
      return false;
    case EME_SESSION_TYPE_NOT_SUPPORTED:
      return false;
    case EME_SESSION_TYPE_SUPPORTED_WITH_PERMISSION:
      return is_permission_granted;
    case EME_SESSION_TYPE_SUPPORTED:
      return true;
  }

  NOTREACHED();
  return false;
}

bool KeySystems::IsPersistentStateRequirementSupported(
    const std::string& key_system,
    EmeFeatureRequirement requirement,
    bool is_permission_granted) {
  DCHECK(thread_checker_.CalledOnValidThread());

  KeySystemInfoMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
    NOTREACHED();
    return false;
  }

  switch (key_system_iter->second.persistent_state_support) {
    case EME_FEATURE_INVALID:
      NOTREACHED();
      return false;
    case EME_FEATURE_NOT_SUPPORTED:
      return requirement != EME_FEATURE_REQUIRED;
    case EME_FEATURE_REQUESTABLE_WITH_PERMISSION:
      return (requirement != EME_FEATURE_REQUIRED) || is_permission_granted;
    case EME_FEATURE_REQUESTABLE:
      return true;
    case EME_FEATURE_ALWAYS_ENABLED:
      // Persistent state does not require user permission, but the session
      // types that use it might.
      return requirement != EME_FEATURE_NOT_ALLOWED;
  }

  NOTREACHED();
  return false;
}

bool KeySystems::IsDistinctiveIdentifierRequirementSupported(
    const std::string& key_system,
    EmeFeatureRequirement requirement,
    bool is_permission_granted) {
  DCHECK(thread_checker_.CalledOnValidThread());

  KeySystemInfoMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
    NOTREACHED();
    return false;
  }

  switch (key_system_iter->second.distinctive_identifier_support) {
    case EME_FEATURE_INVALID:
      NOTREACHED();
      return false;
    case EME_FEATURE_NOT_SUPPORTED:
      return requirement != EME_FEATURE_REQUIRED;
    case EME_FEATURE_REQUESTABLE_WITH_PERMISSION:
      // TODO(sandersd): Remove this hack once crbug.com/457482 and
      // crbug.com/460616 are addressed.
      // We cannot currently enforce "not-allowed", so don't allow it.
      // Note: Removing this check will expose crbug.com/460616.
      if (requirement == EME_FEATURE_NOT_ALLOWED)
        return false;
      return (requirement != EME_FEATURE_REQUIRED) || is_permission_granted;
    case EME_FEATURE_REQUESTABLE:
      NOTREACHED();
      return true;
    case EME_FEATURE_ALWAYS_ENABLED:
      // Distinctive identifiers always require user permission.
      return (requirement != EME_FEATURE_NOT_ALLOWED) && is_permission_granted;
  }

  NOTREACHED();
  return false;
}

void KeySystems::AddContainerMask(const std::string& container, uint32 mask) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!container_to_codec_mask_map_.count(container));
  container_to_codec_mask_map_[container] = static_cast<EmeCodec>(mask);
}

void KeySystems::AddCodecMask(const std::string& codec, uint32 mask) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!codec_string_map_.count(codec));
  codec_string_map_[codec] = static_cast<EmeCodec>(mask);
}

//------------------------------------------------------------------------------

std::string GetUnprefixedKeySystemName(const std::string& key_system) {
  if (key_system == kClearKeyKeySystem)
    return kUnsupportedClearKeyKeySystem;

  if (key_system == kPrefixedClearKeyKeySystem)
    return kClearKeyKeySystem;

  return key_system;
}

std::string GetPrefixedKeySystemName(const std::string& key_system) {
  DCHECK_NE(key_system, kPrefixedClearKeyKeySystem);

  if (key_system == kClearKeyKeySystem)
    return kPrefixedClearKeyKeySystem;

  return key_system;
}

bool IsSaneInitDataTypeWithContainer(
    const std::string& init_data_type,
    const std::string& container) {
  if (init_data_type == "cenc") {
    return container == "audio/mp4" || container == "video/mp4";
  } else if (init_data_type == "webm") {
    return  container == "audio/webm" || container == "video/webm";
  } else {
    return true;
  }
}

bool PrefixedIsSupportedConcreteKeySystem(const std::string& key_system) {
  return KeySystems::GetInstance().IsConcreteSupportedKeySystem(key_system);
}

bool IsSupportedKeySystem(const std::string& key_system) {
  if (!KeySystems::GetInstance().IsSupportedKeySystem(key_system))
    return false;

  // TODO(ddorwin): Move this to where we add key systems when prefixed EME is
  // removed (crbug.com/249976).
  if (!IsPotentiallySupportedKeySystem(key_system)) {
    // If you encounter this path, see the comments for the above function.
    NOTREACHED() << "Unrecognized key system " << key_system
                 << ". See code comments.";
    return false;
  }

  return true;
}

bool IsSupportedKeySystemWithInitDataType(
    const std::string& key_system,
    const std::string& init_data_type) {
  return KeySystems::GetInstance().IsSupportedKeySystemWithInitDataType(
      key_system, init_data_type);
}

bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return KeySystems::GetInstance().IsSupportedKeySystemWithMediaMimeType(
      mime_type, codecs, key_system, false);
}

bool PrefixedIsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return KeySystems::GetInstance().IsSupportedKeySystemWithMediaMimeType(
      mime_type, codecs, key_system, true);
}

std::string GetKeySystemNameForUMA(const std::string& key_system) {
  return KeySystems::GetInstance().GetKeySystemNameForUMA(key_system);
}

bool CanUseAesDecryptor(const std::string& concrete_key_system) {
  return KeySystems::GetInstance().UseAesDecryptor(concrete_key_system);
}

#if defined(ENABLE_PEPPER_CDMS)
std::string GetPepperType(const std::string& concrete_key_system) {
  return KeySystems::GetInstance().GetPepperType(concrete_key_system);
}
#endif

bool IsPersistentLicenseSessionSupported(
    const std::string& key_system,
    bool is_permission_granted) {
  return KeySystems::GetInstance().IsPersistentLicenseSessionSupported(
      key_system, is_permission_granted);
}

bool IsPersistentReleaseMessageSessionSupported(
    const std::string& key_system,
    bool is_permission_granted) {
  return KeySystems::GetInstance().IsPersistentReleaseMessageSessionSupported(
      key_system, is_permission_granted);
}

bool IsPersistentStateRequirementSupported(
    const std::string& key_system,
    EmeFeatureRequirement requirement,
    bool is_permission_granted) {
  return KeySystems::GetInstance().IsPersistentStateRequirementSupported(
      key_system, requirement, is_permission_granted);
}

bool IsDistinctiveIdentifierRequirementSupported(
    const std::string& key_system,
    EmeFeatureRequirement requirement,
    bool is_permission_granted) {
  return KeySystems::GetInstance().IsDistinctiveIdentifierRequirementSupported(
      key_system, requirement, is_permission_granted);
}

// These two functions are for testing purpose only. The declaration in the
// header file is guarded by "#if defined(UNIT_TEST)" so that they can be used
// by tests but not non-test code. However, this .cc file is compiled as part of
// "media" where "UNIT_TEST" is not defined. So we need to specify
// "MEDIA_EXPORT" here again so that they are visible to tests.

MEDIA_EXPORT void AddContainerMask(const std::string& container, uint32 mask) {
  KeySystems::GetInstance().AddContainerMask(container, mask);
}

MEDIA_EXPORT void AddCodecMask(const std::string& codec, uint32 mask) {
  KeySystems::GetInstance().AddCodecMask(codec, mask);
}

}  // namespace media
