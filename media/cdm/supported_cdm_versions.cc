// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/supported_cdm_versions.h"

#include "media/cdm/api/content_decryption_module.h"

namespace media {

bool IsSupportedCdmModuleVersion(int version) {
  switch (version) {
    // Latest.
    case CDM_MODULE_VERSION:
      return true;
    default:
      return false;
  }
}

bool IsSupportedCdmInterfaceVersion(int version) {
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

bool IsSupportedCdmHostVersion(int version) {
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

}  // namespace media
