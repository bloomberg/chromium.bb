// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_INFO_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_INFO_H_

#include <string>

#include "base/basictypes.h"

namespace WebKit {
class WebString;
}

namespace content {

struct MediaFormatAndKeySystem {
  const char* mime_type;
  const char* codecs_list;
  const char* key_system;
};

#if defined(ENABLE_PEPPER_CDMS)
struct KeySystemPepperTypePair {
  const char* key_system;
  const char* type;
};
#endif  // defined(ENABLE_PEPPER_CDMS)

#if defined(OS_ANDROID)
struct KeySystemUUIDPair {
  const char* key_system;
  const uint8 uuid[16];
};
#endif  // defined(OS_ANDROID)

// Specifies the container and codec combinations supported by individual
// key systems. Each line is a container-codecs combination and the key system
// that supports it. Multiple codecs can be listed. In all cases, the container
// without a codec is also supported.
// This list is converted at runtime into individual container-codec-key system
// entries in KeySystems::key_system_map_.
extern const MediaFormatAndKeySystem kSupportedFormatKeySystemCombinations[];
extern const int kNumSupportedFormatKeySystemCombinations;

#if defined(ENABLE_PEPPER_CDMS)
// There should be one entry for each key system.
extern const KeySystemPepperTypePair kKeySystemToPepperTypeMapping[];
extern const int kNumKeySystemToPepperTypeMapping;
#endif  // defined(ENABLE_PEPPER_CDMS)

#if defined(OS_ANDROID)
// Mapping from key system to UUID, one entry per key system.
extern const KeySystemUUIDPair kKeySystemToUUIDMapping[];
extern const int kNumKeySystemToUUIDMapping;
#endif  // defined(OS_ANDROID)

// Returns whether |key_system| is compatible with the user's system.
bool IsSystemCompatible(const std::string& key_system);

// Returns true if canPlayType should return an empty string for |key_system|.
bool IsCanPlayTypeSuppressed(const std::string& key_system);

// Returns the name that UMA will use for the given |key_system|.
// This function can be called frequently. Hence this function should be
// implemented not to impact performance.
std::string KeySystemNameForUMAInternal(const WebKit::WebString& key_system);

// Returns whether built-in AesDecryptor can be used for the given |key_system|.
bool CanUseBuiltInAesDecryptor(const std::string& key_system);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_INFO_H_
