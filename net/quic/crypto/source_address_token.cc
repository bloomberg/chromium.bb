// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/source_address_token.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

using std::string;
using std::vector;

namespace net {

SourceAddressToken::SourceAddressToken() {
}

SourceAddressToken::~SourceAddressToken() {
}

string SourceAddressToken::SerializeAsString() const {
  return ip_ + " " + base::Int64ToString(timestamp_);
}

bool SourceAddressToken::ParseFromArray(unsigned char* plaintext,
                                        size_t plaintext_length) {
  string data(reinterpret_cast<const char*>(plaintext), plaintext_length);
  vector<string> results;
  base::SplitString(data, ' ', &results);
  if (results.size() < 2) {
    return false;
  }

  int64 timestamp;
  if (!base::StringToInt64(results[1], &timestamp)) {
    return false;
  }

  ip_ = results[0];
  timestamp_ = timestamp;
  return true;
}

}  // namespace net
