// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_KEY_INFORMATION_H_
#define MEDIA_BASE_CDM_KEY_INFORMATION_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

struct MEDIA_EXPORT CdmKeyInformation {
  enum KeyStatus {
    USABLE = 0,
    INTERNAL_ERROR = 1,
    EXPIRED = 2,
    OUTPUT_NOT_ALLOWED = 3,
    OUTPUT_DOWNSCALED = 4,
    KEY_STATUS_PENDING = 5,
    KEY_STATUS_MAX = KEY_STATUS_PENDING
  };

  CdmKeyInformation();
  ~CdmKeyInformation();

  std::vector<uint8> key_id;
  KeyStatus status;
  uint32 system_code;
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_KEY_INFORMATION_H_
