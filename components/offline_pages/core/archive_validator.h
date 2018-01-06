// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_VALIDATOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_VALIDATOR_H_

#include <string>

#include "base/macros.h"

namespace base {
class FilePath;
}

namespace offline_pages {

// Contains all helper functions to validate an archive file.
class ArchiveValidator {
 public:
  // Computes a SHA256 digest of the specified file. Empty string will be
  // returned if the digest cannot be computed.
  static std::string ComputeDigest(const base::FilePath& file_path);

  // Returns true if the specified file has |expected_file_size| and
  // |expected_digest|.
  static bool ValidateFile(const base::FilePath& file_path,
                           int64_t expected_file_size,
                           const std::string& expected_digest);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ArchiveValidator);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_VALIDATOR_H_
