// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/public/txt_record.h"

#include "absl/strings/ascii.h"

namespace openscreen {
namespace discovery {

Error DnsSdTxtRecord::SetValue(const std::string& key,
                               const absl::Span<const uint8_t>& value) {
  if (!IsKeyValuePairValid(key, value)) {
    return Error::Code::kParameterInvalid;
  }

  key_value_txt_[key] = std::vector<uint8_t>(value.begin(), value.end());
  ClearFlag(key);
  return Error::None();
}

Error DnsSdTxtRecord::SetFlag(const std::string& key, bool value) {
  if (!IsKeyValid(key)) {
    return Error::Code::kParameterInvalid;
  }

  if (value) {
    boolean_txt_.insert(key);
  } else {
    ClearFlag(key);
  }
  ClearValue(key);
  return Error::None();
}

ErrorOr<absl::Span<const uint8_t>> DnsSdTxtRecord::GetValue(
    const std::string& key) const {
  if (!IsKeyValid(key)) {
    return Error::Code::kParameterInvalid;
  }

  auto it = key_value_txt_.find(key);
  if (it != key_value_txt_.end()) {
    return absl::Span<const uint8_t>(it->second.data(), it->second.size());
  }

  return Error::Code::kItemNotFound;
}

ErrorOr<bool> DnsSdTxtRecord::GetFlag(const std::string& key) const {
  if (!IsKeyValid(key)) {
    return Error::Code::kParameterInvalid;
  }

  return boolean_txt_.find(key) != boolean_txt_.end();
}

Error DnsSdTxtRecord::ClearValue(const std::string& key) {
  if (!IsKeyValid(key)) {
    return Error::Code::kParameterInvalid;
  }

  key_value_txt_.erase(key);
  return Error::None();
}

Error DnsSdTxtRecord::ClearFlag(const std::string& key) {
  if (!IsKeyValid(key)) {
    return Error::Code::kParameterInvalid;
  }

  boolean_txt_.erase(key);
  return Error::None();
}

bool DnsSdTxtRecord::IsKeyValid(const std::string& key) const {
  // The max length of any individual TXT record is 255 bytes.
  if (key.size() > 255) {
    return false;
  }

  // The length of a key must be at least 1.
  if (key.size() < 1) {
    return false;
  }

  // Keys must contain only valid characters.
  for (char key_char : key) {
    if (key_char < char{0x20} || key_char > char{0x7E} || key_char == '=') {
      return false;
    }
  }

  return true;
}

bool DnsSdTxtRecord::IsKeyValuePairValid(
    const std::string& key,
    const absl::Span<const uint8_t>& value) const {
  // The max length of any individual TXT record is 255 bytes.
  if (key.size() + value.size() + 1 /* for equals */ > 255) {
    return false;
  }

  return IsKeyValid(key);
}

bool DnsSdTxtRecord::CaseInsensitiveComparison::operator()(
    const std::string& lhs,
    const std::string& rhs) const {
  return std::less<std::string>()(absl::AsciiStrToLower(lhs),
                                  absl::AsciiStrToLower(rhs));
}

}  // namespace discovery
}  // namespace openscreen
