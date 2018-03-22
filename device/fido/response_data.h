// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_RESPONSE_DATA_H_
#define DEVICE_FIDO_RESPONSE_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"

namespace device {

// Base class for RegisterResponseData and SignResponseData.
class COMPONENT_EXPORT(DEVICE_FIDO) ResponseData {
 public:
  std::string GetId() const;
  const std::vector<uint8_t>& raw_credential_id() const {
    return raw_credential_id_;
  }

 protected:
  explicit ResponseData(std::vector<uint8_t> raw_credential_id);
  ResponseData();

  // Moveable.
  ResponseData(ResponseData&& other);
  ResponseData& operator=(ResponseData&& other);

  ~ResponseData();

  std::vector<uint8_t> raw_credential_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResponseData);
};

}  // namespace device

#endif  // DEVICE_FIDO_RESPONSE_DATA_H_
