// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_AUTHENTICATOR_UTILS_H_
#define DEVICE_U2F_AUTHENTICATOR_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "base/containers/span.h"

namespace device {
namespace u2f_parsing_utils {
// U2FResponse offsets. The format of a U2F response is defined in
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-response-message-success
extern const uint32_t kU2fResponseKeyHandleLengthPos;
extern const uint32_t kU2fResponseKeyHandleStartPos;
extern const char kEs256[];

void Append(std::vector<uint8_t>* target, base::span<const uint8_t> in_values);

// Parses out a sub-vector after verifying no out-of-bound reads.
std::vector<uint8_t> Extract(base::span<const uint8_t> source,
                             size_t pos,
                             size_t length);

}  // namespace u2f_parsing_utils
}  // namespace device

#endif  // DEVICE_U2F_AUTHENTICATOR_UTILS_H_
