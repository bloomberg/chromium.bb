// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_
#define MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_

#include "media/base/media_export.h"
#include "media/cdm/api/content_decryption_module.h"

// A library CDM interface is "supported" if it's implemented by CdmAdapter and
// CdmWrapper. Typically multiple CDM interfaces are supported:
// - The latest stable CDM interface.
// - Previous stable CDM interface(s), for supporting older CDMs.
// - Experimental CDM interface(s), for development.
//
// A library CDM interface is "enabled" if it's enabled at runtime, e.g. being
// able to be registered and creating CDM instances. Typically experimental CDM
// interfaces are supported, but not enabled by default.
//
// Whether a CDM interface is enabled can also be overridden by using command
// line switch switches::kOverrideEnabledCdmInterfaceVersion for finer control
// in a test environment or for local debugging, including enabling experimental
// CDM interfaces.

namespace media {

namespace {

typedef bool (*VersionCheckFunc)(int version);

// Returns true if all versions in the range [min_version, max_version] and no
// versions outside the range are supported, and false otherwise.
constexpr bool CheckSupportedVersions(VersionCheckFunc check_func,
                                      int min_version,
                                      int max_version) {
  // For simplicity, only check one version out of the range boundary.
  if (check_func(min_version - 1) || check_func(max_version + 1))
    return false;

  for (int version = min_version; version <= max_version; ++version) {
    if (!check_func(version))
      return false;
  }

  return true;
}

}  // namespace

// Traits for CDM Interfaces
template <int CdmInterfaceVersion>
struct CdmInterfaceTraits {};

template <>
struct CdmInterfaceTraits<9> {
  using CdmInterface = cdm::ContentDecryptionModule_9;
  static_assert(CdmInterface::kVersion == 9, "CDM interface version mismatch.");
  static constexpr bool IsEnabledByDefault() { return true; }
};

template <>
struct CdmInterfaceTraits<10> {
  using CdmInterface = cdm::ContentDecryptionModule_10;
  static_assert(CdmInterface::kVersion == 10,
                "CDM interface version mismatch.");
  static constexpr bool IsEnabledByDefault() { return false; }
};

// Returns whether the CDM interface of |version| is supported in the
// implementation.
constexpr bool IsSupportedCdmInterfaceVersion(int version) {
  static_assert(cdm::ContentDecryptionModule::kVersion ==
                    cdm::ContentDecryptionModule_9::kVersion,
                "update the code below");
  switch (version) {
    // Supported versions in decreasing order.
    case cdm::ContentDecryptionModule_10::kVersion:
    case cdm::ContentDecryptionModule_9::kVersion:
      return true;
    default:
      return false;
  }
}

// Returns whether the CDM host interface of |version| is supported in the
// implementation. Currently there's no way to disable a supported CDM host
// interface at run time.
constexpr bool IsSupportedCdmHostVersion(int version) {
  static_assert(cdm::ContentDecryptionModule::Host::kVersion ==
                    cdm::ContentDecryptionModule_9::Host::kVersion,
                "update the code below");
  switch (version) {
    // Supported versions in decreasing order.
    case cdm::Host_10::kVersion:
    case cdm::Host_9::kVersion:
      return true;
    default:
      return false;
  }
}

// Ensures CDM interface versions in and only in the range [min_version,
// max_version] are supported in the implementation.
constexpr bool CheckSupportedCdmInterfaceVersions(int min_version,
                                                  int max_version) {
  // The latest stable CDM interface should always be supported.
  int latest_stable_version = cdm::ContentDecryptionModule::kVersion;
  if (latest_stable_version < min_version ||
      latest_stable_version > max_version) {
    return false;
  }

  return CheckSupportedVersions(IsSupportedCdmInterfaceVersion, min_version,
                                max_version);
}

// Ensures CDM host interface versions in and only in the range [min_version,
// max_version] are supported in the implementation.
constexpr bool CheckSupportedCdmHostVersions(int min_version, int max_version) {
  // The latest stable CDM Host interface should always be supported.
  int latest_stable_version = cdm::ContentDecryptionModule::Host::kVersion;
  if (latest_stable_version < min_version ||
      latest_stable_version > max_version) {
    return false;
  }

  return CheckSupportedVersions(IsSupportedCdmHostVersion, min_version,
                                max_version);
}

MEDIA_EXPORT bool IsSupportedCdmModuleVersion(int version);

// Returns whether the CDM interface of |version| is supported in the
// implementation and enabled at runtime.
MEDIA_EXPORT bool IsSupportedAndEnabledCdmInterfaceVersion(int version);

}  // namespace media

#endif  // MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_
