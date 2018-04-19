// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_
#define MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_

#include "media/base/media_export.h"
#include "media/cdm/api/content_decryption_module.h"

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

MEDIA_EXPORT bool IsSupportedCdmModuleVersion(int version);

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
// max_version] are supported.
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
// max_version] are supported.
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

}  // namespace media

#endif  // MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_
