// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_UTILS_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace content {
namespace authenticator_utils {

// JSON key values
constexpr char kTypeKey[] = "type";
constexpr char kChallengeKey[] = "challenge";
constexpr char kOriginKey[] = "origin";
constexpr char kHashAlgorithm[] = "hashAlgorithm";
constexpr char kTokenBindingKey[] = "tokenBinding";

// U2FResponse offsets. The format of a U2F response is defined in
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-response-message-success
constexpr uint32_t kU2fResponseKeyHandleLengthPos = 66;
constexpr uint32_t kU2fResponseKeyHandleStartPos = 67;

constexpr char kEs256[] = "ES256";

void Append(std::vector<uint8_t>* target,
            const std::vector<uint8_t>& in_values);

// Parses out a sub-vector after verifying no out-of-bound reads.
std::vector<uint8_t> Extract(const std::vector<uint8_t>& source,
                             size_t pos,
                             size_t length);

}  // namespace authenticator_utils
}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_UTILS_H_
