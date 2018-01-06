// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/archive_validator.h"

#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace offline_pages {

// static
std::string ArchiveValidator::ComputeDigest(const base::FilePath& file_path) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return std::string();

  std::unique_ptr<crypto::SecureHash> secure_hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  const int kMaxBufferSize = 1024;
  std::vector<char> buffer(kMaxBufferSize);
  int bytes_read;
  do {
    bytes_read = file.ReadAtCurrentPos(buffer.data(), kMaxBufferSize);
    if (bytes_read > 0)
      secure_hash->Update(buffer.data(), bytes_read);
  } while (bytes_read > 0);
  if (bytes_read < 0)
    return std::string();

  std::string result_bytes(crypto::kSHA256Length, 0);
  secure_hash->Finish(&result_bytes[0], result_bytes.size());
  return result_bytes;
}

// static
bool ArchiveValidator::ValidateFile(const base::FilePath& file_path,
                                    int64_t expected_file_size,
                                    const std::string& expected_digest) {
  int64_t actual_file_size;
  if (!base::GetFileSize(file_path, &actual_file_size))
    return false;
  if (expected_file_size != actual_file_size)
    return false;

  std::string actual_digest = ComputeDigest(file_path);
  return expected_digest == actual_digest;
}

}  // namespace offline_pages
