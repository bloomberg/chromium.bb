// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/key_systems.h"

#include <string>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/media/crypto/key_systems_support_uma.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_info.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_bridge.h"
#endif

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

namespace content {

using media::EmeCodec;
using media::EmeInitDataType;
using media::KeySystemInfo;
using media::SupportedInitDataTypes;
using media::SupportedCodecs;

const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kPrefixedClearKeyKeySystem[] = "webkit-org.w3.clearkey";
const char kUnsupportedClearKeyKeySystem[] = "unsupported-org.w3.clearkey";

struct NamedInitDataType {
  const char* name;
  EmeInitDataType type;
};

// Mapping between initialization data types names and enum values. When adding
// entries, make sure to update IsSaneInitDataTypeWithContainer().
static NamedInitDataType kInitDataTypeNames[] = {
    {"webm", media::EME_INIT_DATA_TYPE_WEBM},
#if defined(USE_PROPRIETARY_CODECS)
    {"cenc", media::EME_INIT_DATA_TYPE_CENC}
#endif  // defined(USE_PROPRIETARY_CODECS)
};

struct NamedCodec {
  const char* name;
  EmeCodec type;
};

// Mapping between containers and their codecs.
// Only audio codec can belong to a "audio/*" container. Both audio and video
// codecs can belong to a "video/*" container.
static NamedCodec kContainerToCodecMasks[] = {
    {"audio/webm", media::EME_CODEC_WEBM_AUDIO_ALL},
    {"video/webm", media::EME_CODEC_WEBM_ALL},
#if defined(USE_PROPRIETARY_CODECS)
    {"audio/mp4", media::EME_CODEC_MP4_AUDIO_ALL},
    {"video/mp4", media::EME_CODEC_MP4_ALL}
#endif  // defined(USE_PROPRIETARY_CODECS)
};

// Mapping between codec names and enum values.
static NamedCodec kCodecStrings[] = {
    {"vorbis", media::EME_CODEC_WEBM_VORBIS},
    {"vp8", media::EME_CODEC_WEBM_VP8},
    {"vp8.0", media::EME_CODEC_WEBM_VP8},
    {"vp9", media::EME_CODEC_WEBM_VP9},
    {"vp9.0", media::EME_CODEC_WEBM_VP9},
#if defined(USE_PROPRIETARY_CODECS)
    {"mp4a", media::EME_CODEC_MP4_AAC},
    {"avc1", media::EME_CODEC_MP4_AVC1},
    {"avc3", media::EME_CODEC_MP4_AVC1}
#endif  // defined(USE_PROPRIETARY_CODECS)
};

static void AddClearKey(std::vector<KeySystemInfo>* concrete_key_systems) {
  KeySystemInfo info(kClearKeyKeySystem);

  // On Android, Vorbis, VP8, AAC and AVC1 are supported in MediaCodec:
  // http://developer.android.com/guide/appendix/media-formats.html
  // VP9 support is device dependent.

  info.supported_init_data_types = media::EME_INIT_DATA_TYPE_WEBM;
  info.supported_codecs = media::EME_CODEC_WEBM_ALL;

#if defined(OS_ANDROID)
  // Temporarily disable VP9 support for Android.
  // TODO(xhwang): Use mime_util.h to query VP9 support on Android.
  info.supported_codecs &= ~media::EME_CODEC_WEBM_VP9;
#endif  // defined(OS_ANDROID)

#if defined(USE_PROPRIETARY_CODECS)
  info.supported_init_data_types |= media::EME_INIT_DATA_TYPE_CENC;
  info.supported_codecs |= media::EME_CODEC_MP4_ALL;
#endif  // defined(USE_PROPRIETARY_CODECS)

  info.use_aes_decryptor = true;

  concrete_key_systems->push_back(info);
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
      const std::string& key_system);

  bool UseAesDecryptor(const std::string& concrete_key_system);

#if defined(ENABLE_PEPPER_CDMS)
  std::string GetPepperType(const std::string& concrete_key_system);
#endif

  void AddContainerMask(const std::string& container, uint32 mask);
  void AddCodecMask(const std::string& codec, uint32 mask);

 private:
  void UpdateSupportedKeySystems();

  void AddConcreteSupportedKeySystems(
      const std::vector<KeySystemInfo>& concrete_key_systems);

  void AddConcreteSupportedKeySystem(
      const std::string& key_system,
      bool use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
      const std::string& pepper_type,
#endif
      SupportedInitDataTypes supported_init_data_types,
      SupportedCodecs supported_codecs,
      const std::string& parent_key_system);

  friend struct base::DefaultLazyInstanceTraits<KeySystems>;

  struct KeySystemProperties {
    KeySystemProperties()
        : use_aes_decryptor(false), supported_codecs(media::EME_CODEC_NONE) {}

    bool use_aes_decryptor;
#if defined(ENABLE_PEPPER_CDMS)
    std::string pepper_type;
#endif
    SupportedInitDataTypes supported_init_data_types;
    SupportedCodecs supported_codecs;
  };

  typedef base::hash_map<std::string, KeySystemProperties>
      KeySystemPropertiesMap;
  typedef base::hash_map<std::string, std::string> ParentKeySystemMap;
  typedef base::hash_map<std::string, SupportedCodecs> ContainerCodecsMap;
  typedef base::hash_map<std::string, EmeCodec> CodecsMap;
  typedef base::hash_map<std::string, media::EmeInitDataType> InitDataTypesMap;

  KeySystems();
  ~KeySystems() {}

  EmeInitDataType GetInitDataTypeForName(
      const std::string& init_data_type) const;
  // TODO(sandersd): Separate container enum from codec mask value.
  // http://crbug.com/417440
  SupportedCodecs GetCodecMaskForContainer(
      const std::string& container) const;
  EmeCodec GetCodecForString(const std::string& codec) const;

  const std::string& GetConcreteKeySystemName(
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
  KeySystemPropertiesMap concrete_key_system_map_;

  // Map from parent key system to the concrete key system that should be used
  // to represent its capabilities.
  ParentKeySystemMap parent_key_system_map_;

  KeySystemsSupportUMA key_systems_support_uma_;

  InitDataTypesMap init_data_type_name_map_;
  ContainerCodecsMap container_to_codec_mask_map_;
  CodecsMap codec_string_map_;

  bool needs_update_;
  base::Time last_update_time_;

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
KeySystems::KeySystems() : needs_update_(true) {
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

  UpdateSupportedKeySystems();

#if defined(WIDEVINE_CDM_AVAILABLE)
  key_systems_support_uma_.AddKeySystemToReport(kWidevineKeySystem);
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
}

EmeInitDataType KeySystems::GetInitDataTypeForName(
    const std::string& init_data_type) const {
  InitDataTypesMap::const_iterator iter =
      init_data_type_name_map_.find(init_data_type);
  if (iter != init_data_type_name_map_.end())
    return iter->second;
  return media::EME_INIT_DATA_TYPE_NONE;
}

SupportedCodecs KeySystems::GetCodecMaskForContainer(
    const std::string& container) const {
  ContainerCodecsMap::const_iterator iter =
      container_to_codec_mask_map_.find(container);
  if (iter != container_to_codec_mask_map_.end())
    return iter->second;
  return media::EME_CODEC_NONE;
}

EmeCodec KeySystems::GetCodecForString(const std::string& codec) const {
  CodecsMap::const_iterator iter = codec_string_map_.find(codec);
  if (iter != codec_string_map_.end())
    return iter->second;
  return media::EME_CODEC_NONE;
}

const std::string& KeySystems::GetConcreteKeySystemName(
    const std::string& key_system) const {
  ParentKeySystemMap::const_iterator iter =
      parent_key_system_map_.find(key_system);
  if (iter != parent_key_system_map_.end())
    return iter->second;
  return key_system;
}

void KeySystems::UpdateIfNeeded() {
#if defined(WIDEVINE_CDM_AVAILABLE)
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!needs_update_)
    return;

  // The update could involve a sync IPC to the browser process. Use a minimum
  // update interval to avoid unnecessary frequent IPC to the browser.
  static const int kMinUpdateIntervalInSeconds = 1;
  base::Time now = base::Time::Now();
  if (now - last_update_time_ <
      base::TimeDelta::FromSeconds(kMinUpdateIntervalInSeconds)) {
    return;
  }

  UpdateSupportedKeySystems();
#endif
}

void KeySystems::UpdateSupportedKeySystems() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(needs_update_);
  concrete_key_system_map_.clear();
  parent_key_system_map_.clear();

  // Build KeySystemInfo.
  std::vector<KeySystemInfo> key_systems_info;
  GetContentClient()->renderer()->AddKeySystems(&key_systems_info);
  // Clear Key is always supported.
  AddClearKey(&key_systems_info);

  AddConcreteSupportedKeySystems(key_systems_info);

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  if (IsConcreteSupportedKeySystem(kWidevineKeySystem))
    needs_update_ = false;
#endif

  last_update_time_ = base::Time::Now();
}

void KeySystems::AddConcreteSupportedKeySystems(
    const std::vector<KeySystemInfo>& concrete_key_systems) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(concrete_key_system_map_.empty());
  DCHECK(parent_key_system_map_.empty());

  for (size_t i = 0; i < concrete_key_systems.size(); ++i) {
    const KeySystemInfo& key_system_info = concrete_key_systems[i];
    AddConcreteSupportedKeySystem(key_system_info.key_system,
                                  key_system_info.use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
                                  key_system_info.pepper_type,
#endif
                                  key_system_info.supported_init_data_types,
                                  key_system_info.supported_codecs,
                                  key_system_info.parent_key_system);
  }
}

void KeySystems::AddConcreteSupportedKeySystem(
    const std::string& concrete_key_system,
    bool use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
    const std::string& pepper_type,
#endif
    SupportedInitDataTypes supported_init_data_types,
    SupportedCodecs supported_codecs,
    const std::string& parent_key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!IsConcreteSupportedKeySystem(concrete_key_system))
      << "Key system '" << concrete_key_system << "' already registered";
  DCHECK(!parent_key_system_map_.count(concrete_key_system))
      <<  "'" << concrete_key_system << " is already registered as a parent";

  KeySystemProperties properties;
  properties.use_aes_decryptor = use_aes_decryptor;
#if defined(ENABLE_PEPPER_CDMS)
  DCHECK_EQ(use_aes_decryptor, pepper_type.empty());
  properties.pepper_type = pepper_type;
#endif

  properties.supported_init_data_types = supported_init_data_types;
  properties.supported_codecs = supported_codecs;

  concrete_key_system_map_[concrete_key_system] = properties;

  if (!parent_key_system.empty()) {
    DCHECK(!IsConcreteSupportedKeySystem(parent_key_system))
        << "Parent '" << parent_key_system << "' already registered concrete";
    DCHECK(!parent_key_system_map_.count(parent_key_system))
        << "Parent '" << parent_key_system << "' already registered";
    parent_key_system_map_[parent_key_system] = concrete_key_system;
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
  return (concrete_key_system_map_.count(key_system) ||
          parent_key_system_map_.count(key_system));
}

bool KeySystems::IsSupportedKeySystemWithInitDataType(
    const std::string& key_system,
    const std::string& init_data_type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If |key_system| is a parent key system, use its concrete child.
  const std::string& concrete_key_system = GetConcreteKeySystemName(key_system);

  // Locate |concrete_key_system|.
  KeySystemPropertiesMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end())
    return false;

  // Check |init_data_type| and |key_system| x |init_data_type|.
  const KeySystemProperties& properties = key_system_iter->second;
  EmeInitDataType eme_init_data_type = GetInitDataTypeForName(init_data_type);
  return (properties.supported_init_data_types & eme_init_data_type) != 0;
}

// TODO(sandersd): Reorganize to be more similar to
// IsKeySystemSupportedWithInitDataType(). Note that a fork may still be
// required; http://crbug.com/417461.
bool KeySystems::IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If |key_system| is a parent key system, use its concrete child.
  const std::string& concrete_key_system = GetConcreteKeySystemName(key_system);

  bool has_type = !mime_type.empty();

  key_systems_support_uma_.ReportKeySystemQuery(key_system, has_type);

  // Check key system support.
  KeySystemPropertiesMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end())
    return false;

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

  key_systems_support_uma_.ReportKeySystemSupport(key_system, true);
  return true;
}

bool KeySystems::UseAesDecryptor(const std::string& concrete_key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  KeySystemPropertiesMap::const_iterator key_system_iter =
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

  KeySystemPropertiesMap::const_iterator key_system_iter =
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

bool IsConcreteSupportedKeySystem(const std::string& key_system) {
  return KeySystems::GetInstance().IsConcreteSupportedKeySystem(key_system);
}

bool IsSupportedKeySystem(const std::string& key_system) {
  return KeySystems::GetInstance().IsSupportedKeySystem(key_system);
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
      mime_type, codecs, key_system);
}

std::string KeySystemNameForUMA(const std::string& key_system) {
  if (key_system == kClearKeyKeySystem)
    return "ClearKey";
#if defined(WIDEVINE_CDM_AVAILABLE)
  if (key_system == kWidevineKeySystem)
    return "Widevine";
#endif  // WIDEVINE_CDM_AVAILABLE
  return "Unknown";
}

bool CanUseAesDecryptor(const std::string& concrete_key_system) {
  return KeySystems::GetInstance().UseAesDecryptor(concrete_key_system);
}

#if defined(ENABLE_PEPPER_CDMS)
std::string GetPepperType(const std::string& concrete_key_system) {
  return KeySystems::GetInstance().GetPepperType(concrete_key_system);
}
#endif

// These two functions are for testing purpose only. The declaration in the
// header file is guarded by "#if defined(UNIT_TEST)" so that they can be used
// by tests but not non-test code. However, this .cc file is compiled as part of
// "content" where "UNIT_TEST" is not defined. So we need to specify
// "CONTENT_EXPORT" here again so that they are visible to tests.

CONTENT_EXPORT void AddContainerMask(const std::string& container,
                                     uint32 mask) {
  KeySystems::GetInstance().AddContainerMask(container, mask);
}

CONTENT_EXPORT void AddCodecMask(const std::string& codec, uint32 mask) {
  KeySystems::GetInstance().AddCodecMask(codec, mask);
}

}  // namespace content
