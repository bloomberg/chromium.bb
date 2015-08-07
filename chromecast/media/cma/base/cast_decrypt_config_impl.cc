// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/cast_decrypt_config_impl.h"

#include "media/base/decrypt_config.h"

namespace chromecast {
namespace media {

CastDecryptConfigImpl::CastDecryptConfigImpl(
    const ::media::DecryptConfig& config)
    : key_id_(config.key_id()), iv_(config.iv()) {
  for (const auto& sample : config.subsamples()) {
    subsamples_.push_back(
        SubsampleEntry(sample.clear_bytes, sample.cypher_bytes));
  }
}

CastDecryptConfigImpl::CastDecryptConfigImpl(
    const std::string& key_id,
    const std::string& iv,
    const std::vector<SubsampleEntry>& subsamples)
    : key_id_(key_id), iv_(iv), subsamples_(subsamples) {}

CastDecryptConfigImpl::~CastDecryptConfigImpl() {}

const std::string& CastDecryptConfigImpl::key_id() const {
  return key_id_;
}

const std::string& CastDecryptConfigImpl::iv() const {
  return iv_;
}

const std::vector<SubsampleEntry>& CastDecryptConfigImpl::subsamples() const {
  return subsamples_;
}

}  // namespace media
}  // namespace chromecast
