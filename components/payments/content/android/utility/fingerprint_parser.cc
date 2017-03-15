// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/utility/fingerprint_parser.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace payments {
namespace {

bool IsUpperCaseHexDigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

uint8_t HexDigitToByte(char c) {
  DCHECK(IsUpperCaseHexDigit(c));
  return base::checked_cast<uint8_t>(c >= '0' && c <= '9' ? c - '0'
                                                          : c - 'A' + 10);
}

}  // namespace

std::vector<uint8_t> FingerprintStringToByteArray(const std::string& input) {
  std::vector<uint8_t> output;
  if (input.size() != 32 * 3 - 1)
    return output;

  for (size_t i = 0; i < input.size(); i += 3) {
    if (i < input.size() - 2 && input[i + 2] != ':') {
      output.clear();
      return output;
    }

    char big_end = input[i];
    char little_end = input[i + 1];
    if (!IsUpperCaseHexDigit(big_end) || !IsUpperCaseHexDigit(little_end)) {
      output.clear();
      return output;
    }

    output.push_back(HexDigitToByte(big_end) * static_cast<uint8_t>(16) +
                     HexDigitToByte(little_end));
  }

  return output;
}

}  // namespace payments
