// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/digest_verifier.h"

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/resource_util.h"
#include "chrome/chrome_cleaner/proto/file_digest.pb.h"

namespace chrome_cleaner {

// static
std::shared_ptr<DigestVerifier> DigestVerifier::CreateFromResource(
    int resource_id) {
  std::shared_ptr<DigestVerifier> instance(new DigestVerifier());
  if (!instance->Initialize(resource_id)) {
    LOG(ERROR) << "Failed to initialize DigestVerifier from resource "
               << resource_id;
    return nullptr;
  }
  return instance;
}

DigestVerifier::~DigestVerifier() = default;

bool DigestVerifier::IsKnownFile(const base::FilePath& file) const {
  const auto digest_entry =
      digests_.find(base::ToLowerASCII(file.BaseName().value()));
  if (digest_entry == digests_.end())
    return false;

  std::string actual_digest;
  if (!chrome_cleaner::ComputeDigestSHA256(file, &actual_digest)) {
    LOG(ERROR) << "Failed to compute digest for " << SanitizePath(file);
    return false;
  }

  return (base::ToLowerASCII(actual_digest) == digest_entry->second);
}

DigestVerifier::DigestVerifier() = default;

bool DigestVerifier::Initialize(int resource_id) {
  base::StringPiece serialized_digest_pb;
  if (!chrome_cleaner::LoadResourceOfKind(resource_id, L"TEXT",
                                          &serialized_digest_pb)) {
    LOG(DFATAL) << "Failed to load expected digests from resources";
    return false;
  }

  chrome_cleaner::FileDigests digests_pb;
  if (!digests_pb.ParseFromString(serialized_digest_pb.as_string())) {
    LOG(ERROR) << "Failed to parse digests protobuf";
    return false;
  }

  for (const chrome_cleaner::FileDigest& digest : digests_pb.file_digests()) {
    const base::string16 filename = base::UTF8ToUTF16(digest.filename());
    digests_[base::ToLowerASCII(filename)] =
        base::ToLowerASCII(digest.digest());
  }

  return true;
}

std::vector<base::FilePath::StringType> DigestVerifier::GetKnownFileNames()
    const {
  std::vector<base::FilePath::StringType> result;
  result.reserve(digests_.size());
  for (auto iter = digests_.begin(); iter != digests_.end(); ++iter) {
    result.push_back(iter->first);
  }
  return result;
}

}  // namespace chrome_cleaner
