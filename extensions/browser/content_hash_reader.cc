// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_reader.h"

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
      key_(key) {
}

ContentHashReader::~ContentHashReader() {
}

bool ContentHashReader::Init() {
  return true;
}

int ContentHashReader::block_count() const {
  return 0;
}

int ContentHashReader::block_size() const {
  return 0;
}

bool ContentHashReader::GetHashForBlock(int block_index,
                                        const std::string** result) const {
  return false;
}

}  // namespace extensions
