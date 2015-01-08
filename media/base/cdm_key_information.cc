// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_key_information.h"

namespace media {

CdmKeyInformation::CdmKeyInformation()
    : status(INTERNAL_ERROR), system_code(0) {
}

CdmKeyInformation::~CdmKeyInformation() {
}

}  // namespace media
