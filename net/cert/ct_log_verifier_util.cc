// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier_util.h"

#include <memory>

#include "base/strings/string_util.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace net {

namespace ct {

namespace internal {

std::string HashNodes(const std::string& lh, const std::string& rh) {
  std::unique_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  hash->Update("\01", 1);
  hash->Update(lh.data(), lh.size());
  hash->Update(rh.data(), rh.size());

  std::string result;
  hash->Finish(base::WriteInto(&result, crypto::kSHA256Length + 1),
               crypto::kSHA256Length);
  return result;
}

}  // namespace internal

}  // namespace ct

}  // namespace net
