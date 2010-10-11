// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/crypto_helpers.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/base64.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

using std::string;
using std::vector;

MD5Calculator::MD5Calculator() {
  MD5Init(&context_);
}

MD5Calculator::~MD5Calculator() {}

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

const vector<uint8>& MD5Calculator::GetDigest() {
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
  uint64 random_int;
  const char* random_int_bytes = reinterpret_cast<const char*>(&random_int);
  int random_int_size = sizeof(random_int);
  for (int i = 0; i < output_length; i += random_int_size) {
    random_int = base::RandUint64();
    int copy_count = std::min(output_length - i, random_int_size);
    memcpy(output + i, random_int_bytes, copy_count);
  }
}

string Generate128BitRandomHexString() {
  const int kNumberBytes = 128 / 8;
  std::string random_bytes(kNumberBytes, ' ');
  GetRandomBytes(&random_bytes[0], kNumberBytes);
  std::string base64_encoded_bytes;
  base::Base64Encode(random_bytes, &base64_encoded_bytes);
  return base64_encoded_bytes;
}
