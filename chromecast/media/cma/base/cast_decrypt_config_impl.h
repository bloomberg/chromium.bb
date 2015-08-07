// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_CAST_DECRYPT_CONFIG_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BASE_CAST_DECRYPT_CONFIG_IMPL_H_

#include "chromecast/public/media/cast_decrypt_config.h"

namespace media {
class DecryptConfig;
}

namespace chromecast {
namespace media {

// Contains all information that a decryptor needs to decrypt a media sample.
class CastDecryptConfigImpl : public CastDecryptConfig {
 public:
  CastDecryptConfigImpl(const ::media::DecryptConfig& config);
  CastDecryptConfigImpl(const std::string& key_id,
                        const std::string& iv,
                        const std::vector<SubsampleEntry>& subsamples);
  ~CastDecryptConfigImpl() override;

  const std::string& key_id() const override;
  const std::string& iv() const override;
  const std::vector<SubsampleEntry>& subsamples() const override;

 private:
  std::string key_id_;
  std::string iv_;
  std::vector<SubsampleEntry> subsamples_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_CAST_DECRYPT_CONFIG_IMPL_H_
