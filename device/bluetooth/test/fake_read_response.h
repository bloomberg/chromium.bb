// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_TEST_FAKE_READ_RESPONSE_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_READ_RESPONSE_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"

namespace bluetooth {

// Holds the necessary values for dispatching a fake read response.
//
// Not intended for direct use by clients.  See README.md.
class FakeReadResponse {
 public:
  FakeReadResponse(uint16_t gatt_code,
                   const base::Optional<std::vector<uint8_t>>& value);
  ~FakeReadResponse();

  uint16_t gatt_code() { return gatt_code_; }
  const base::Optional<std::vector<uint8_t>>& value() { return value_; }

 private:
  uint16_t gatt_code_;
  base::Optional<std::vector<uint8_t>> value_;

  DISALLOW_COPY_AND_ASSIGN(FakeReadResponse);
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_READ_RESPONSE_H_
