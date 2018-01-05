// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_parsing_utils.h"

#include "base/logging.h"

namespace device {
namespace u2f_parsing_utils {

const uint32_t kU2fResponseKeyHandleLengthPos = 66u;
const uint32_t kU2fResponseKeyHandleStartPos = 67u;
const char kEs256[] = "ES256";

void Append(std::vector<uint8_t>* target, base::span<const uint8_t> in_values) {
  target->insert(target->end(), in_values.begin(), in_values.end());
}

std::vector<uint8_t> Extract(base::span<const uint8_t> source,
                             size_t pos,
                             size_t length) {
  if (!(pos <= source.size() && length <= source.size() - pos)) {
    return std::vector<uint8_t>();
  }

  return std::vector<uint8_t>(source.begin() + pos,
                              source.begin() + pos + length);
}

}  // namespace u2f_parsing_utils
}  // namespace device
