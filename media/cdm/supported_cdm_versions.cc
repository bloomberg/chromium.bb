// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/supported_cdm_versions.h"

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

}  // namespace media
