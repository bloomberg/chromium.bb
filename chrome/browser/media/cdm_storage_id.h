// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CDM_STORAGE_ID_H_
#define CHROME_BROWSER_MEDIA_CDM_STORAGE_ID_H_

#include <stdint.h>

#include <vector>

#include "base/callback_forward.h"
#include "url/origin.h"

// This handles computing the Storage Id for platform verification.
namespace cdm_storage_id {

using CdmStorageIdCallback =
    base::OnceCallback<void(const std::vector<uint8_t>& storage_id)>;

// Compute the Storage Id based on |salt| and |origin|. This may be
// asynchronous, so call |callback| with the result. If Storage Id is not
// supported on the current platform, an empty string will be passed to
// |callback|.
void ComputeStorageId(const std::vector<uint8_t>& salt,
                      const url::Origin& origin,
                      CdmStorageIdCallback callback);

}  // namespace cdm_storage_id

#endif  // CHROME_BROWSER_MEDIA_CDM_STORAGE_ID_H_
