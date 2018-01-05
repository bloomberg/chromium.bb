// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_CTAP_CTAP_REQUEST_PARAM_H_
#define DEVICE_CTAP_CTAP_REQUEST_PARAM_H_

#include <stdint.h>
#include <vector>

#include "base/optional.h"

namespace device {

class CTAPRequestParam {
 public:
  CTAPRequestParam();
  virtual ~CTAPRequestParam();
  virtual base::Optional<std::vector<uint8_t>> SerializeToCBOR() const = 0;
};

}  // namespace device

#endif  // DEVICE_CTAP_CTAP_REQUEST_PARAM_H_
