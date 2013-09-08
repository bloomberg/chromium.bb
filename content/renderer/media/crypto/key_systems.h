// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace WebKit {
class WebString;
}

// Definitions:
// * Key system
//    https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#key-system
// * Concrete key system
//    A key system string that can be instantiated, such as
//    via the MediaKeys constructor. Examples include "org.w3.clearkey" and
//    "com.widevine.alpha".
// * Abstract key system
//    A key system string that cannot be instantiated like a concrete key system
//    but is otherwise useful, such as in discovery using isTypeSupported().
// * Parent key system
//    A key system string that is one level up from the child key system. It may
//    be an abstract key system.
//    As an example, "com.example" is the parent of "com.example.foo".

namespace content {

// Adds a concrete key system along with platform-specific information about how
// to instantiate it. Must be called before AddSupportedType().
// May only be called once per |concrete_key_system|.
// When not empty, |parent_key_system| will add a mapping to the
// |concrete_key_system| that can be used to check supported types.
// Only one parent key system is currently supported per concrete key system.
CONTENT_EXPORT void AddConcreteSupportedKeySystem(
    const std::string& concrete_key_system,
    bool use_aes_decryptor,
#if defined(ENABLE_PEPPER_CDMS)
    const std::string& pepper_type,
#elif defined(OS_ANDROID)
    const uint8 uuid[16],
#endif
    const std::string& parent_key_system);

// Specifies the container and codec combinations supported by
// |concrete_key_system|.
// Multiple codecs can be listed. In all cases, the container
// without a codec is also supported.
// |concrete_key_system| must be a concrete supported key system previously
// added using AddConcreteSupportedKeySystem().
CONTENT_EXPORT void AddSupportedType(const std::string& concrete_key_system,
                                     const std::string& mime_type,
                                     const std::string& codecs_list);

// Returns whether |key_system| is a real supported key system that can be
// instantiated.
// Abstract parent |key_system| strings will return false.
// Call IsSupportedKeySystemWithMediaMimeType() to determine whether a
// |key_system| supports a specific type of media or to check parent key
// systems.
CONTENT_EXPORT bool IsConcreteSupportedKeySystem(
    const WebKit::WebString& key_system);

// Returns whether |key_sytem| supports the specified media type and codec(s).
CONTENT_EXPORT bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system);

// Returns a name for |key_system| suitable to UMA logging.
CONTENT_EXPORT std::string KeySystemNameForUMA(
    const WebKit::WebString& key_system);

// Returns whether AesDecryptor can be used for the given |concrete_key_system|.
CONTENT_EXPORT bool CanUseAesDecryptor(const std::string& concrete_key_system);

#if defined(ENABLE_PEPPER_CDMS)
// Returns the Pepper MIME type for |concrete_key_system|.
// Returns empty string if |concrete_key_system| is unknown or not Pepper-based.
CONTENT_EXPORT std::string GetPepperType(
    const std::string& concrete_key_system);
#elif defined(OS_ANDROID)
// Convert |concrete_key_system| to 16-byte Android UUID.
CONTENT_EXPORT std::vector<uint8> GetUUID(
    const std::string& concrete_key_system);
#endif

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_
