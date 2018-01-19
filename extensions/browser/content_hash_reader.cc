// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_reader.h"

#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_hash_tree.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/verified_contents.h"

namespace extensions {

ContentHashReader::ContentHashReader(const std::string& extension_id,
                                     const base::Version& extension_version,
                                     const base::FilePath& extension_root,
                                     const base::FilePath& relative_path,
                                     const ContentVerifierKey& key)
    : extension_id_(extension_id),
      extension_version_(extension_version.GetString()),
      extension_root_(extension_root),
      relative_path_(relative_path),
      key_(key) {}

ContentHashReader::~ContentHashReader() {
}

bool ContentHashReader::Init() {
  base::ElapsedTimer timer;
  DCHECK_EQ(status_, NOT_INITIALIZED);
  status_ = FAILURE;

  std::unique_ptr<ContentHash> content_hash =
      ContentHash::Create(ContentHash::ExtensionKey(
          extension_id_, extension_root_, extension_version_, key_));

  if (!content_hash->succeeded())
    return false;

  has_content_hashes_ = true;

  const VerifiedContents& verified_contents = content_hash->verified_contents();

  // Extensions sometimes request resources that do not have an entry in
  // verified_contents.json. This can happen when an extension sends an XHR to a
  // resource.
  if (!verified_contents.HasTreeHashRoot(relative_path_)) {
    // Making a request to a non-existent resource should not result in
    // content verification failure.
    // TODO(proberge): The relative_path_.empty() check should be moved higher
    // in the execution flow for performance wins by saving on costly IO
    // operations and calculations.
    if (relative_path_.empty() ||
        !base::PathExists(extension_root_.Append(relative_path_)))
      file_missing_from_verified_contents_ = true;

    return false;
  }

  const ComputedHashes::Reader& reader = content_hash->computed_hashes();
  if (!reader.GetHashes(relative_path_, &block_size_, &hashes_) ||
      block_size_ % crypto::kSHA256Length != 0) {
    return false;
  }

  std::string root =
      ComputeTreeHashRoot(hashes_, block_size_ / crypto::kSHA256Length);
  if (!verified_contents.TreeHashRootEquals(relative_path_, root))
    return false;

  status_ = SUCCESS;
  UMA_HISTOGRAM_TIMES("ExtensionContentHashReader.InitLatency",
                      timer.Elapsed());
  return true;
}

int ContentHashReader::block_count() const {
  DCHECK(status_ != NOT_INITIALIZED);
  return hashes_.size();
}

int ContentHashReader::block_size() const {
  DCHECK(status_ != NOT_INITIALIZED);
  return block_size_;
}

bool ContentHashReader::GetHashForBlock(int block_index,
                                        const std::string** result) const {
  if (status_ != SUCCESS)
    return false;
  DCHECK(block_index >= 0);

  if (static_cast<unsigned>(block_index) >= hashes_.size())
    return false;
  *result = &hashes_[block_index];

  return true;
}

}  // namespace extensions
