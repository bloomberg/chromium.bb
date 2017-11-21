// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_utils.h"

#include "base/logging.h"

namespace content {
namespace authenticator_utils {

void Append(std::vector<uint8_t>* target,
            const std::vector<uint8_t>& in_values) {
  target->insert(target->end(), in_values.begin(), in_values.end());
}

std::vector<uint8_t> Extract(const std::vector<uint8_t>& source,
                             size_t pos,
                             size_t length) {
  if (!(pos <= source.size() && length <= source.size() - pos)) {
    return std::vector<uint8_t>();
  }

  return std::vector<uint8_t>(source.begin() + pos,
                              source.begin() + pos + length);
}

}  // namespace authenticator_utils
}  // namespace content
