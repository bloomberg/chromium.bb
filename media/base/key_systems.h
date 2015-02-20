// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEMS_H_
#define MEDIA_BASE_KEY_SYSTEMS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "media/base/eme_constants.h"
#include "media/base/media_export.h"

namespace media {

// Prefixed EME API only supports prefixed (webkit-) key system name for
// certain key systems. But internally only unprefixed key systems are
// supported. The following two functions help convert between prefixed and
// unprefixed key system names.

// Gets the unprefixed key system name for |key_system|.
MEDIA_EXPORT std::string GetUnprefixedKeySystemName(
    const std::string& key_system);

// Gets the prefixed key system name for |key_system|.
MEDIA_EXPORT std::string GetPrefixedKeySystemName(
    const std::string& key_system);

// Returns false if a container-specific |init_data_type| is specified with an
// inappropriate container.
// TODO(sandersd): Remove this essentially internal detail if the spec is
// updated to not convolve the two in a single method call.
// TODO(sandersd): Use enum values instead of strings. http://crbug.com/417440
MEDIA_EXPORT bool IsSaneInitDataTypeWithContainer(
    const std::string& init_data_type,
    const std::string& container);

// Use for unprefixed EME only!
// Returns whether |key_system| is a supported key system.
// Note: Shouldn't be used for prefixed API as the original
// IsSupportedKeySystemWithMediaMimeType() path reports UMAs, but this path does
// not.
MEDIA_EXPORT bool IsSupportedKeySystem(const std::string& key_system);

MEDIA_EXPORT bool IsSupportedKeySystemWithInitDataType(
    const std::string& key_system,
    const std::string& init_data_type);

// Use for prefixed EME only!
// Returns whether |key_system| is a real supported key system that can be
// instantiated.
// Abstract parent |key_system| strings will return false.
// Call IsSupportedKeySystemWithMediaMimeType() to determine whether a
// |key_system| supports a specific type of media or to check parent key
// systems.
MEDIA_EXPORT bool PrefixedIsSupportedConcreteKeySystem(
    const std::string& key_system);

// Use for unprefixed EME only!
// Returns whether |key_system| supports the specified media type and codec(s).
MEDIA_EXPORT bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system);

// Use for prefixed EME only!
// Returns whether |key_system| supports the specified media type and codec(s).
// To be used with prefixed EME only as it generates UMAs based on the query.
MEDIA_EXPORT bool PrefixedIsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system);

// Returns a name for |key_system| suitable to UMA logging.
MEDIA_EXPORT std::string GetKeySystemNameForUMA(const std::string& key_system);

// Returns whether AesDecryptor can be used for the given |concrete_key_system|.
MEDIA_EXPORT bool CanUseAesDecryptor(const std::string& concrete_key_system);

#if defined(ENABLE_PEPPER_CDMS)
// Returns the Pepper MIME type for |concrete_key_system|.
// Returns empty string if |concrete_key_system| is unknown or not Pepper-based.
MEDIA_EXPORT std::string GetPepperType(
    const std::string& concrete_key_system);
#endif

// Returns whether |key_system| supports persistent-license sessions.
MEDIA_EXPORT bool IsPersistentLicenseSessionSupported(
    const std::string& key_system,
    bool is_permission_granted);

// Returns whether |key_system| supports persistent-release-message sessions.
MEDIA_EXPORT bool IsPersistentReleaseMessageSessionSupported(
    const std::string& key_system,
    bool is_permission_granted);

// Returns whether |key_system| supports persistent state as requested.
MEDIA_EXPORT bool IsPersistentStateRequirementSupported(
    const std::string& key_system,
    EmeFeatureRequirement requirement,
    bool is_permission_granted);

// Returns whether |key_system| supports distinctive identifiers as requested.
MEDIA_EXPORT bool IsDistinctiveIdentifierRequirementSupported(
    const std::string& key_system,
    EmeFeatureRequirement requirement,
    bool is_permission_granted);

#if defined(UNIT_TEST)
// Helper functions to add container/codec types for testing purposes.
MEDIA_EXPORT void AddContainerMask(const std::string& container, uint32 mask);
MEDIA_EXPORT void AddCodecMask(const std::string& codec, uint32 mask);
#endif  // defined(UNIT_TEST)

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEMS_H_
