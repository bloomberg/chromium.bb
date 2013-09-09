// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/key_systems.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/renderer/media/crypto/key_systems_info.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

// Convert a WebString to ASCII, falling back on an empty string in the case
// of a non-ASCII string.
static std::string ToASCIIOrEmpty(const WebKit::WebString& string) {
  return IsStringASCII(string) ? UTF16ToASCII(string) : std::string();
}

class KeySystems {
 public:
  static KeySystems* GetInstance();

  void AddConcreteSupportedKeySystem(
      const std::string& key_system,
      bool use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
      const std::string& pepper_type,
#elif defined(OS_ANDROID)
      const uint8 uuid[16],
#endif
      const std::string& parent_key_system);

  void AddSupportedType(const std::string& key_system,
                        const std::string& mime_type,
                        const std::string& codecs_list);

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

  KeySystems() {}

  bool IsSupportedKeySystemWithContainerAndCodec(
      const std::string& mime_type,
      const std::string& codec,
      const std::string& key_system);

  // Map from key system string to capabilities.
  KeySystemPropertiesMap concrete_key_system_map_;

  // Map from parent key system to the concrete key system that should be used
  // to represent its capabilities.
  ParentKeySystemMap parent_key_system_map_;

  DISALLOW_COPY_AND_ASSIGN(KeySystems);
};

static base::LazyInstance<KeySystems> g_key_systems = LAZY_INSTANCE_INITIALIZER;

KeySystems* KeySystems::GetInstance() {
  KeySystems* key_systems = &g_key_systems.Get();
  // TODO(ddorwin): Call out to ContentClient to register key systems.
  static bool is_registered = false;
  if (!is_registered) {
    is_registered = true;  // Prevent reentrancy when Add*() is called.
    RegisterKeySystems();
  }
  return key_systems;
}

void KeySystems::AddConcreteSupportedKeySystem(
    const std::string& concrete_key_system,
    bool use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
    const std::string& pepper_type,
#elif defined(OS_ANDROID)
    const uint8 uuid[16],
#endif
    const std::string& parent_key_system) {
  DCHECK(!IsConcreteSupportedKeySystem(concrete_key_system))
      << "Key system '" << concrete_key_system << "' already registered";

  KeySystemProperties properties;
  properties.use_aes_decryptor = use_aes_decryptor;
#if defined(ENABLE_PEPPER_CDMS)
  DCHECK_EQ(use_aes_decryptor, pepper_type.empty());
  properties.pepper_type = pepper_type;
#elif defined(OS_ANDROID)
  // Since |uuid| can't be empty, use |use_aes_decryptor|.
  if (!use_aes_decryptor)
    properties.uuid.assign(uuid, uuid + 16);
#endif
  concrete_key_system_map_[concrete_key_system] = properties;

  if (!parent_key_system.empty()) {
    DCHECK(parent_key_system_map_.find(parent_key_system) ==
           parent_key_system_map_.end())
        << "Parent '" << parent_key_system.c_str() << "' already registered.";
    parent_key_system_map_[parent_key_system] = concrete_key_system;
  }
}

void KeySystems::AddSupportedType(const std::string& concrete_key_system,
                                  const std::string& mime_type,
                                  const std::string& codecs_list) {
  std::vector<std::string> mime_type_codecs;
  net::ParseCodecString(codecs_list, &mime_type_codecs, false);

  CodecSet codecs(mime_type_codecs.begin(), mime_type_codecs.end());
  // Support the MIME type string alone, without codec(s) specified.
  codecs.insert(std::string());

  KeySystemPropertiesMap::iterator key_system_iter =
      concrete_key_system_map_.find(concrete_key_system);
  DCHECK(key_system_iter != concrete_key_system_map_.end());
  MimeTypeMap& mime_types_map = key_system_iter->second.types;
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
  KeySystemPropertiesMap::const_iterator key_system_iter =
      concrete_key_system_map_.find(key_system);
  if (key_system_iter == concrete_key_system_map_.end())
    return false;

  const MimeTypeMap& mime_types_map = key_system_iter->second.types;
  MimeTypeMap::const_iterator mime_iter = mime_types_map.find(mime_type);
  if (mime_iter == mime_types_map.end())
    return false;

  const CodecSet& codecs = mime_iter->second;
  return (codecs.find(codec) != codecs.end());
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

  // This method is only used by the canPlaytType() path (not the EME methods),
  // so we check for suppressed key_systems here.
  if(IsCanPlayTypeSuppressed(concrete_key_system))
    return false;

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

void AddConcreteSupportedKeySystem(
    const std::string& key_system,
    bool use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
    const std::string& pepper_type,
#elif defined(OS_ANDROID)
    const uint8 uuid[16],
#endif
    const std::string& parent_key_system) {
  KeySystems::GetInstance()->AddConcreteSupportedKeySystem(key_system,
                                                           use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
                                                           pepper_type,
#elif defined(OS_ANDROID)
                                                           uuid,
#endif
                                                           parent_key_system);
}

void AddSupportedType(const std::string& key_system,
                      const std::string& mime_type,
                      const std::string& codecs_list) {
  KeySystems::GetInstance()->AddSupportedType(key_system,
                                              mime_type, codecs_list);
}

bool IsConcreteSupportedKeySystem(const WebKit::WebString& key_system) {
  return KeySystems::GetInstance()->IsConcreteSupportedKeySystem(
      ToASCIIOrEmpty(key_system));
}

bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return KeySystems::GetInstance()->IsSupportedKeySystemWithMediaMimeType(
      mime_type, codecs, key_system);
}

std::string KeySystemNameForUMA(const WebKit::WebString& key_system) {
  return KeySystemNameForUMAInternal(key_system);
}

bool CanUseAesDecryptor(const std::string& concrete_key_system) {
  return KeySystems::GetInstance()->UseAesDecryptor(concrete_key_system);
}

#if defined(ENABLE_PEPPER_CDMS)
std::string GetPepperType(const std::string& concrete_key_system) {
  return KeySystems::GetInstance()->GetPepperType(concrete_key_system);
}
#elif defined(OS_ANDROID)
std::vector<uint8> GetUUID(const std::string& concrete_key_system) {
  return KeySystems::GetInstance()->GetUUID(concrete_key_system);
}
#endif

}  // namespace content
