// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_SECTION_CETS_ECM_H_
#define MEDIA_FORMATS_MP2T_TS_SECTION_CETS_ECM_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/byte_queue.h"
#include "media/formats/mp2t/ts_section.h"

namespace media {

class DecryptConfig;

namespace mp2t {

class TsSectionCetsEcm : public TsSection {
 public:
  // RegisterDecryptConfigCb::Run(const DecryptConfig& decrypt_config);
  using RegisterDecryptConfigCb =
      base::Callback<void(const DecryptConfig& decrypt_config)>;
  explicit TsSectionCetsEcm(
      const RegisterDecryptConfigCb& register_decrypt_config_cb);
  ~TsSectionCetsEcm() override;

  // TsSection implementation.
  bool Parse(bool payload_unit_start_indicator,
             const uint8_t* buf,
             int size) override;
  void Flush() override;
  void Reset() override;

 private:
  RegisterDecryptConfigCb register_decrypt_config_cb_;

  DISALLOW_COPY_AND_ASSIGN(TsSectionCetsEcm);
};

}  // namespace mp2t
}  // namespace media

#endif
