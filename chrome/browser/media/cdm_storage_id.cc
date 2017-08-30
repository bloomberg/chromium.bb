// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_storage_id.h"

#include "base/callback.h"

namespace cdm_storage_id {

void ComputeStorageId(const std::vector<uint8_t>& salt,
                      const url::Origin& origin,
                      CdmStorageIdCallback callback) {
  // Not implemented by default.
  std::move(callback).Run(std::vector<uint8_t>());
}

}  // namespace cdm_storage_id
