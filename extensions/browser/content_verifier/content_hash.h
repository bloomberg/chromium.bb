// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_HASH_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_HASH_H_

#include <set>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/version.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_verifier/content_verifier_key.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Represents content verification hashes for an extension.
//
// Instances can be created using Create() factory methods on sequences with
// blocking IO access. If hash retrieval succeeds then ContentHash::succeeded()
// will return true and
// a. ContentHash::verified_contents() will return structured representation of
//    verified_contents.json
// b. ContentHash::computed_hashes() will return structured representation of
//    computed_hashes.json.
//
// Additionally if computed_hashes.json was required to be written to disk and
// it was successful, ContentHash::hash_mismatch_unix_paths() will return all
// FilePaths from the extension directory that had content verification
// mismatch.
class ContentHash {
 public:
  // Key to identify an extension.
  struct ExtensionKey {
    ExtensionId extension_id;
    base::FilePath extension_root;
    base::Version extension_version;
    // The key used to validate verified_contents.json.
    ContentVerifierKey verifier_key;

    ExtensionKey(const ExtensionId& extension_id,
                 const base::FilePath& extension_root,
                 const base::Version& extension_version,
                 const ContentVerifierKey& verifier_key);

    ExtensionKey(const ExtensionKey& other);
    ExtensionKey& operator=(const ExtensionKey& other);
  };

  // Specifiable modes for ContentHash instantiation.
  enum Mode {
    // Deletes verified_contents.json if the file on disk is invalid.
    kDeleteInvalidVerifiedContents = 1 << 0,

    // Always creates computed_hashes.json (as opposed to only when the file is
    // non-existent).
    kForceRecreateExistingComputedHashesFile = 1 << 1,
  };

  using IsCancelledCallback = base::RepeatingCallback<bool(void)>;

  // Factories:
  // These will never return nullptr, but verified_contents or computed_hashes
  // may be empty if something fails.
  // Reads existing hashes from disk.
  static std::unique_ptr<ContentHash> Create(const ExtensionKey& key);
  // Reads existing hashes from disk, with specifying flags from |Mode|.
  static std::unique_ptr<ContentHash> Create(
      const ExtensionKey& key,
      int mode,
      const IsCancelledCallback& is_cancelled);

  ~ContentHash();

  const VerifiedContents& verified_contents() const;
  const ComputedHashes::Reader& computed_hashes() const;

  bool has_verified_contents() const {
    return status_ >= Status::kHasVerifiedContents;
  }
  bool succeeded() const { return status_ >= Status::kSucceeded; }

  // If ContentHash creation writes computed_hashes.json, then this returns the
  // FilePaths whose content hash didn't match expected hashes.
  const std::set<base::FilePath>& hash_mismatch_unix_paths() {
    return hash_mismatch_unix_paths_;
  }

 private:
  enum class Status {
    // Retrieving hashes failed.
    kInvalid,
    // Retrieved valid verified_contents.json, but there was no
    // computed_hashes.json.
    kHasVerifiedContents,
    // Both verified_contents.json and computed_hashes.json were read
    // correctly.
    kSucceeded,
  };

  ContentHash(const ExtensionKey& key,
              std::unique_ptr<VerifiedContents> verified_contents,
              std::unique_ptr<ComputedHashes::Reader> computed_hashes);

  static std::unique_ptr<ContentHash> CreateImpl(
      const ExtensionKey& key,
      int mode,
      bool create_computed_hashes_file,
      const IsCancelledCallback& is_cancelled);

  // Computes hashes for all files in |key_.extension_root|, and uses
  // a ComputedHashes::Writer to write that information into |hashes_file|.
  // Returns true on success.
  // The verified contents file from the webstore only contains the treehash
  // root hash, but for performance we want to cache the individual block level
  // hashes. This function will create that cache with block-level hashes for
  // each file in the extension if needed (the treehash root hash for each of
  // these should equal what is in the verified contents file from the
  // webstore).
  bool CreateHashes(const base::FilePath& hashes_file,
                    const IsCancelledCallback& is_cancelled);

  ExtensionKey key_;

  Status status_;

  // TODO(lazyboy): Avoid dynamic allocations here, |this| already supports
  // move.
  std::unique_ptr<VerifiedContents> verified_contents_;
  std::unique_ptr<ComputedHashes::Reader> computed_hashes_;

  // Paths that were found to have a mismatching hash.
  std::set<base::FilePath> hash_mismatch_unix_paths_;

  // The block size to use for hashing.
  // TODO(asargent) - use the value from verified_contents.json for each
  // file, instead of using a constant.
  int block_size_ = extension_misc::kContentVerificationDefaultBlockSize;

  DISALLOW_COPY_AND_ASSIGN(ContentHash);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_HASH_H_
