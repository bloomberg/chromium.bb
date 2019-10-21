// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_TXT_RECORD_H_
#define DISCOVERY_DNSSD_PUBLIC_TXT_RECORD_H_

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

class DnsSdTxtRecord {
 public:
  // Sets the value currently stored in this DNS-SD TXT record. Returns error
  // if the provided key is already set or if either the key or value is
  // invalid, and Error::None() otherwise. Case of the key is ignored.
  Error SetValue(const absl::string_view& key,
                 const absl::Span<uint8_t>& value);
  Error SetFlag(const absl::string_view& key, bool value);

  // Reads the value associated with the provided key, or an error if the key
  // is invalid or not present. Case of the key is ignored.
  ErrorOr<absl::Span<uint8_t>> GetValue(const absl::string_view& key) const;
  ErrorOr<bool> GetFlag(const absl::string_view& key) const;

  // Clears an existing TxtRecord value associated with the given key.
  void ClearValue(const absl::string_view& key);
  void ClearFlag(const absl::string_view& key);
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_TXT_RECORD_H_
