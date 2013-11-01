// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_SUPPORTED_CDM_VERSIONS_H_
#define MEDIA_CDM_PPAPI_SUPPORTED_CDM_VERSIONS_H_

#include "media/cdm/ppapi/api/content_decryption_module.h"

namespace media {

// TODO(ddorwin): Move to content_decryption_module.h.
#define CDM_MODULE_VERSION 4

bool IsSupportedCdmModuleVersion(int version) {
  switch(version) {
    // Latest.
    case CDM_MODULE_VERSION:
      return true;
    default:
      return false;
  }
}

bool IsSupportedCdmInterfaceVersion(int version) {
  COMPILE_ASSERT(cdm::ContentDecryptionModule::kVersion ==
                 cdm::ContentDecryptionModule_2::kVersion,
                 update_code_below);
  switch(version) {
    // Latest.
    case cdm::ContentDecryptionModule::kVersion:
    // Older supported versions.
    case cdm::ContentDecryptionModule_1::kVersion:
      return true;
    default:
      return false;
  }
}

bool IsSupportedCdmHostVersion(int version) {
  COMPILE_ASSERT(cdm::ContentDecryptionModule::Host::kVersion ==
                 cdm::ContentDecryptionModule_2::Host::kVersion,
                 update_code_below);
  switch(version) {
    // Supported versions in increasing order (there is no default).
    case cdm::Host_1::kVersion:
    case cdm::Host_2::kVersion:
      return true;
    default:
      return false;
  }
}

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_SUPPORTED_CDM_VERSIONS_H_
