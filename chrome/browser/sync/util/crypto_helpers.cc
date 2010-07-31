// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/crypto_helpers.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

using std::string;
using std::vector;

MD5Calculator::MD5Calculator() {
  MD5Init(&context_);
}

void MD5Calculator::AddData(const unsigned char* data, int length) {
  CHECK(bin_digest_.empty());
  MD5Update(&context_, data, length);
}

void MD5Calculator::CalcDigest() {
  if (bin_digest_.empty()) {
    MD5Digest digest;
    MD5Final(&digest, &context_);
    bin_digest_.assign(digest.a, digest.a + arraysize(digest.a));
  }
}

vector<uint8> MD5Calculator::GetDigest() {
  CalcDigest();
  return bin_digest_;
}

std::string MD5Calculator::GetHexDigest() {
  CalcDigest();
  string hex = base::HexEncode(reinterpret_cast<char*>(&bin_digest_.front()),
                               bin_digest_.size());
  StringToLowerASCII(&hex);
  return hex;
}

void GetRandomBytes(char* output, int output_length) {
  for (int i = 0; i < output_length; i++) {
    // TODO(chron): replace this with something less stupid.
    output[i] = static_cast<char>(base::RandUint64());
  }
}

string Generate128BitRandomHexString() {
  int64 chunk1 = static_cast<int64>(base::RandUint64());
  int64 chunk2 = static_cast<int64>(base::RandUint64());
  return StringPrintf("%016" PRId64 "x%016" PRId64 "x", chunk1, chunk2);
}
