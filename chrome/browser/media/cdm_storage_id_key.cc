// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_storage_id_key.h"

#include "media/media_features.h"

#if !BUILDFLAG(ENABLE_CDM_STORAGE_ID)
#error This should only be compiled if "enable_cdm_storage_id" specified.
#endif

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/internal/google_chrome_cdm_storage_id_key.h"
#endif

std::string GetCdmStorageIdKey() {
#if defined(CDM_STORAGE_ID_KEY)
  return CDM_STORAGE_ID_KEY;
#else
  // For non-Google-Chrome builds, the GN flag "alternate_cdm_storage_id_key"
  // must be set if "enable_cdm_storage_id" specified. See comments in
  // media/media_options.gni.
  return BUILDFLAG(ALTERNATE_CDM_STORAGE_ID_KEY);
#endif
}
