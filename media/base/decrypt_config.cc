// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decrypt_config.h"

#include "base/logging.h"

namespace media {

DecryptConfig::DecryptConfig(const std::string& key_id,
                             const std::string& iv,
                             const std::vector<SubsampleEntry>& subsamples)
    : key_id_(key_id),
      iv_(iv),
      subsamples_(subsamples) {
  CHECK_GT(key_id.size(), 0u);
  CHECK(iv.size() == static_cast<size_t>(DecryptConfig::kDecryptionKeySize) ||
        iv.empty());
}

DecryptConfig::~DecryptConfig() {}

bool DecryptConfig::Matches(const DecryptConfig& config) const {
  if (key_id() != config.key_id() || iv() != config.iv() ||
      subsamples().size() != config.subsamples().size()) {
    return false;
  }

  for (size_t i = 0; i < subsamples().size(); ++i) {
    if ((subsamples()[i].clear_bytes != config.subsamples()[i].clear_bytes) ||
        (subsamples()[i].cypher_bytes != config.subsamples()[i].cypher_bytes)) {
      return false;
    }
  }

  return true;
}

}  // namespace media
