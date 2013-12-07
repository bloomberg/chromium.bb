// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/key_systems.h"

#include <map>
#include <string>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/key_system_info.h"
#include "content/renderer/media/crypto/key_systems_support_uma.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

namespace content {

const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";

const char kAudioWebM[] = "audio/webm";
const char kVideoWebM[] = "video/webm";
const char kVorbis[] = "vorbis";
const char kVorbisVP8[] = "vorbis,vp8,vp8.0";

#if defined(USE_PROPRIETARY_CODECS)
const char kAudioMp4[] = "audio/mp4";
const char kVideoMp4[] = "video/mp4";
const char kMp4a[] = "mp4a";
const char kMp4aAvc1Avc3[] = "mp4a,avc1,avc3";
#endif  // defined(USE_PROPRIETARY_CODECS)

#if !defined(GOOGLE_TV)
inline std::string KeySystemNameForUMAInternal(
    const blink::WebString& key_system) {
  if (key_system == kClearKeyKeySystem)
    return "ClearKey";
#if defined(WIDEVINE_CDM_AVAILABLE)
  if (key_system == kWidevineKeySystem)
    return "Widevine";
#endif  // WIDEVINE_CDM_AVAILABLE
  return "Unknown";
}
#else
// Declares the function, which is defined in another file.
std::string KeySystemNameForUMAInternal(const blink::WebString& key_system);
#endif  // !defined(GOOGLE_TV)

// Convert a WebString to ASCII, falling back on an empty string in the case
// of a non-ASCII string.
static std::string ToASCIIOrEmpty(const blink::WebString& string) {
  return IsStringASCII(string) ? UTF16ToASCII(string) : std::string();
}

static void AddClearKey(std::vector<KeySystemInfo>* concrete_key_systems) {
  KeySystemInfo info(kClearKeyKeySystem);

  info.supported_types.push_back(std::make_pair(kAudioWebM, kVorbis));
  info.supported_types.push_back(std::make_pair(kVideoWebM, kVorbisVP8));
#if defined(USE_PROPRIETARY_CODECS)
  info.supported_types.push_back(std::make_pair(kAudioMp4, kMp4a));
  info.supported_types.push_back(std::make_pair(kVideoMp4, kMp4aAvc1Avc3));
#endif  // defined(USE_PROPRIETARY_CODECS)

  info.use_aes_decryptor = true;

  concrete_key_systems->push_back(info);
}

class KeySystems {
 public:
  static KeySystems& GetInstance();

  bool IsConcreteSupportedKeySystem(const std::string& key_system);

  bool IsSupportedKeySystemWithMediaMimeType(
      const std::string& mime_type,
      const std::vector<std::string>& codecs,
      const std::string& key_system);

  bool UseAesDecryptor(const std::string& concrete_key_system);

#if defined(ENABLE_PEPPER_CDMS)
  std::string GetPepperType(const std::string& concrete_key_system);
#elif defined(OS_ANDROID)
  std::vector<uint8> GetUUID(const std::string& concrete_key_system);
#endif

 private:
  void AddConcreteSupportedKeySystems(
      const std::vector<KeySystemInfo>& concrete_key_systems);

  void AddConcreteSupportedKeySystem(
      const std::string& key_system,
      bool use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
      const std::string& pepper_type,
#elif defined(OS_ANDROID)
      const std::vector<uint8>& uuid,
#endif
      const std::vector<KeySystemInfo::ContainerCodecsPair>& supported_types,
      const std::string& parent_key_system);


  friend struct base::DefaultLazyInstanceTraits<KeySystems>;

  typedef base::hash_set<std::string> CodecSet;
  typedef std::map<std::string, CodecSet> MimeTypeMap;

  struct KeySystemProperties {
    KeySystemProperties() : use_aes_decryptor(false) {}

    bool use_aes_decryptor;
#if defined(ENABLE_PEPPER_CDMS)
    std::string pepper_type;
#elif defined(OS_ANDROID)
    std::vector<uint8> uuid;
#endif
    MimeTypeMap types;
  };

  typedef std::map<std::string, KeySystemProperties> KeySystemPropertiesMap;

  typedef std::map<std::string, std::string> ParentKeySystemMap;

  KeySystems();
  ~KeySystems() {}

  void AddSupportedType(const std::string& mime_type,
                        const std::string& codecs_list,
                        KeySystemProperties* properties);

  bool IsSupportedKeySystemWithContainerAndCodec(const std::string& mime_type,
                                                 const std::string& codec,
                                                 const std::string& key_system);

  // Map from key system string to capabilities.
  KeySystemPropertiesMap concrete_key_system_map_;

  // Map from parent key system to the concrete key system that should be used
  // to represent its capabilities.
  ParentKeySystemMap parent_key_system_map_;

  KeySystemsSupportUMA key_systems_support_uma_;

  DISALLOW_COPY_AND_ASSIGN(KeySystems);
};

static base::LazyInstance<KeySystems> g_key_systems = LAZY_INSTANCE_INITIALIZER;

KeySystems& KeySystems::GetInstance() {
  return g_key_systems.Get();
}

// Because we use a LazyInstance, the key systems info must be populated when
// the instance is lazily initiated.
KeySystems::KeySystems() {
  std::vector<KeySystemInfo> key_systems_info;
  GetContentClient()->renderer()->AddKeySystems(&key_systems_info);
  // Clear Key is always supported.
  AddClearKey(&key_systems_info);
  AddConcreteSupportedKeySystems(key_systems_info);
#if defined(WIDEVINE_CDM_AVAILABLE)
  key_systems_support_uma_.AddKeySystemToReport(kWidevineKeySystem);
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
}

void KeySystems::AddConcreteSupportedKeySystems(
    const std::vector<KeySystemInfo>& concrete_key_systems) {
  for (size_t i = 0; i < concrete_key_systems.size(); ++i) {
    const KeySystemInfo& key_system_info = concrete_key_systems[i];
    AddConcreteSupportedKeySystem(key_system_info.key_system,
                                  key_system_info.use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
                                  key_system_info.pepper_type,
#elif defined(OS_ANDROID)
                                  key_system_info.uuid,
#endif
                                  key_system_info.supported_types,
                                  key_system_info.parent_key_system);
  }
}

void KeySystems::AddConcreteSupportedKeySystem(
    const std::string& concrete_key_system,
    bool use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
    const std::string& pepper_type,
#elif defined(OS_ANDROID)
    const std::vector<uint8>& uuid,
#endif
    const std::vector<KeySystemInfo::ContainerCodecsPair>& supported_types,
    const std::string& parent_key_system) {
  DCHECK(!IsConcreteSupportedKeySystem(concrete_key_system))
      << "Key system '" << concrete_key_system << "' already registered";
  DCHECK(parent_key_system_map_.find(concrete_key_system) ==
         parent_key_system_map_.end())
      <<  "'" << concrete_key_system << " is already registered as a parent";

  KeySystemProperties properties;
  properties.use_aes_decryptor = use_aes_decryptor;
#if defined(ENABLE_PEPPER_CDMS)
  DCHECK_EQ(use_aes_decryptor, pepper_type.empty());
  properties.pepper_type = pepper_type;
#elif defined(OS_ANDROID)
  DCHECK_EQ(use_aes_decryptor, uuid.empty());
  DCHECK(use_aes_decryptor || uuid.size() == 16);
  properties.uuid = uuid;
#endif

  for (size_t i = 0; i < supported_types.size(); ++i) {
    const KeySystemInfo::ContainerCodecsPair& pair = supported_types[i];
    const std::string& mime_type = pair.first;
    const std::string& codecs_list = pair.second;
    AddSupportedType(mime_type, codecs_list, &properties);
  }

  concrete_key_system_map_[concrete_key_system] = properties;

  if (!parent_key_system.empty()) {
    DCHECK(!IsConcreteSupportedKeySystem(parent_key_system))
        << "Parent '" << parent_key_system << "' already registered concrete";
    DCHECK(parent_key_system_map_.find(parent_key_system) ==
           parent_key_system_map_.end())
        << "Parent '" << parent_key_system << "' already registered";
    parent_key_system_map_[parent_key_system] = concrete_key_system;
  }
}

void KeySystems::AddSupportedType(const std::string& mime_type,
                                  const std::string& codecs_list,
                                  KeySystemProperties* properties) {
  std::vector<std::string> mime_type_codecs;
  net::ParseCodecString(codecs_list, &mime_type_codecs, false);

  CodecSet codecs(mime_type_codecs.begin(), mime_type_codecs.end());

  MimeTypeMap& mime_types_map = properties->types;
  // mime_types_map must not be repeated for a given key system.
  DCHECK(mime_types_map.find(mime_type) == mime_types_map.end());
  mime_types_map[mime_type] = codecs;
}

bool KeySystems::IsConcreteSupportedKeySystem(const std::string& key_system) {
  return concrete_key_system_map_.find(key_system) !=
      concrete_key_system_map_.end();
}

bool KeySystems::IsSupportedKeySystemWithContainerAndCodec(
    const std::string& mime_type,
    const std::string& codec,
    const std::string& key_system) {
  bool has_type = !mime_type.empty();
  DCHECK(has_type || codec.empty());

  key_systems_support_uma_.ReportKeySystemQuery(key_system, has_type);

  KeySystemPropertiesMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(key_system);
  if (key_system_iter == concrete_key_system_map_.end())
    return false;

  key_systems_support_uma_.ReportKeySystemSupport(key_system, false);

  if (mime_type.empty())
    return true;

  const MimeTypeMap& mime_types_map = key_system_iter->second.types;
  MimeTypeMap::const_iterator mime_iter = mime_types_map.find(mime_type);
  if (mime_iter == mime_types_map.end())
    return false;

  if (codec.empty()) {
    key_systems_support_uma_.ReportKeySystemSupport(key_system, true);
    return true;
  }

  const CodecSet& codecs = mime_iter->second;
  if (codecs.find(codec) == codecs.end())
    return false;

  key_systems_support_uma_.ReportKeySystemSupport(key_system, true);
  return true;
}

bool KeySystems::IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  // If |key_system| is a parent key_system, use its concrete child.
  // Otherwise, use |key_system|.
  std::string concrete_key_system;
  ParentKeySystemMap::iterator parent_key_system_iter =
      parent_key_system_map_.find(key_system);
  if (parent_key_system_iter != parent_key_system_map_.end())
    concrete_key_system = parent_key_system_iter->second;
  else
    concrete_key_system = key_system;

  if (codecs.empty()) {
    return IsSupportedKeySystemWithContainerAndCodec(
        mime_type, std::string(), concrete_key_system);
  }

  for (size_t i = 0; i < codecs.size(); ++i) {
    if (!IsSupportedKeySystemWithContainerAndCodec(
             mime_type, codecs[i], concrete_key_system)) {
      return false;
    }
  }

  return true;
}

bool KeySystems::UseAesDecryptor(const std::string& concrete_key_system) {
  KeySystemPropertiesMap::iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
      DLOG(FATAL) << concrete_key_system << " is not a known concrete system";
      return false;
  }

  return key_system_iter->second.use_aes_decryptor;
}

#if defined(ENABLE_PEPPER_CDMS)
std::string KeySystems::GetPepperType(const std::string& concrete_key_system) {
  KeySystemPropertiesMap::iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
      DLOG(FATAL) << concrete_key_system << " is not a known concrete system";
      return std::string();
  }

  const std::string& type = key_system_iter->second.pepper_type;
  DLOG_IF(FATAL, type.empty()) << concrete_key_system << " is not Pepper-based";
  return type;
}
#elif defined(OS_ANDROID)
std::vector<uint8> KeySystems::GetUUID(const std::string& concrete_key_system) {
  KeySystemPropertiesMap::iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  if (key_system_iter == concrete_key_system_map_.end()) {
      DLOG(FATAL) << concrete_key_system << " is not a known concrete system";
      return std::vector<uint8>();
  }

  return key_system_iter->second.uuid;
}
#endif

//------------------------------------------------------------------------------

bool IsConcreteSupportedKeySystem(const blink::WebString& key_system) {
  return KeySystems::GetInstance().IsConcreteSupportedKeySystem(
      ToASCIIOrEmpty(key_system));
}

bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return KeySystems::GetInstance().IsSupportedKeySystemWithMediaMimeType(
      mime_type, codecs, key_system);
}

std::string KeySystemNameForUMA(const blink::WebString& key_system) {
  return KeySystemNameForUMAInternal(key_system);
}

std::string KeySystemNameForUMA(const std::string& key_system) {
  return KeySystemNameForUMAInternal(blink::WebString::fromUTF8(key_system));
}

bool CanUseAesDecryptor(const std::string& concrete_key_system) {
  return KeySystems::GetInstance().UseAesDecryptor(concrete_key_system);
}

#if defined(ENABLE_PEPPER_CDMS)
std::string GetPepperType(const std::string& concrete_key_system) {
  return KeySystems::GetInstance().GetPepperType(concrete_key_system);
}
#elif defined(OS_ANDROID)
std::vector<uint8> GetUUID(const std::string& concrete_key_system) {
  return KeySystems::GetInstance().GetUUID(concrete_key_system);
}
#endif

}  // namespace content
