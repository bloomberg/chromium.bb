// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_parsing_utils.h"

#include "base/logging.h"

namespace device {
namespace u2f_parsing_utils {

const uint32_t kU2fResponseKeyHandleLengthPos = 66u;
const uint32_t kU2fResponseKeyHandleStartPos = 67u;
const char kEs256[] = "ES256";

std::vector<uint8_t> Materialize(base::span<const uint8_t> span) {
  return std::vector<uint8_t>(span.begin(), span.end());
}

base::Optional<std::vector<uint8_t>> MaterializeOrNull(
    base::Optional<base::span<const uint8_t>> span) {
  if (span)
    return Materialize(*span);
  return base::nullopt;
}

void Append(std::vector<uint8_t>* target, base::span<const uint8_t> in_values) {
  target->insert(target->end(), in_values.begin(), in_values.end());
}

std::vector<uint8_t> Extract(base::span<const uint8_t> span,
                             size_t pos,
                             size_t length) {
  return Materialize(ExtractSpan(span, pos, length));
}

base::span<const uint8_t> ExtractSpan(base::span<const uint8_t> span,
                                      size_t pos,
                                      size_t length) {
  if (!(pos <= span.size() && length <= span.size() - pos))
    return base::span<const uint8_t>();
  return span.subspan(pos, length);
}

std::vector<uint8_t> ExtractSuffix(base::span<const uint8_t> span, size_t pos) {
  return Materialize(ExtractSuffixSpan(span, pos));
}

base::span<const uint8_t> ExtractSuffixSpan(base::span<const uint8_t> span,
                                            size_t pos) {
  if (pos > span.size())
    return std::vector<uint8_t>();
  return span.subspan(pos);
}

}  // namespace u2f_parsing_utils
}  // namespace device
