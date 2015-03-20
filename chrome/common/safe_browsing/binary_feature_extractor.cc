// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/binary_feature_extractor.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace safe_browsing {

void BinaryFeatureExtractor::ExtractDigest(
    const base::FilePath& file_path,
    ClientDownloadRequest_Digests* digests) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (file.IsValid()) {
    const int kBufferSize = 1 << 12;
    scoped_ptr<char[]> buf(new char[kBufferSize]);
    scoped_ptr<crypto::SecureHash> ctx(
        crypto::SecureHash::Create(crypto::SecureHash::SHA256));
    int len = 0;
    while (true) {
      len = file.ReadAtCurrentPos(buf.get(), kBufferSize);
      if (len <= 0)
        break;
      ctx->Update(buf.get(), len);
    }
    if (!len) {
      uint8_t hash[crypto::kSHA256Length];
      ctx->Finish(hash, sizeof(hash));
      digests->set_sha256(hash, sizeof(hash));
    }
  }
}

}  // namespace safe_browsing
