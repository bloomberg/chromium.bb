// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sha2.h"

#include "base/crypto/secure_hash.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"

namespace base {

void SHA256HashString(const std::string& str, void* output, size_t len) {
  scoped_ptr<base::SecureHash> ctx(base::SecureHash::Create(
      base::SecureHash::SHA256));
  ctx->Update(str.data(), str.length());
  ctx->Finish(output, len);
}

std::string SHA256HashString(const std::string& str) {
  std::string output(SHA256_LENGTH, 0);
  SHA256HashString(str, string_as_array(&output), output.size());
  return output;
}

}  // namespace base
