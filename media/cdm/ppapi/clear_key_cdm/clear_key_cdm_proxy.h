// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_CLEAR_KEY_CDM_CLEAR_KEY_CDM_PROXY_H_
#define MEDIA_CDM_PPAPI_CLEAR_KEY_CDM_CLEAR_KEY_CDM_PROXY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "media/cdm/cdm_proxy.h"

namespace media {

// CdmProxy implementation for Clear Key CDM to test CDM Proxy support.
class ClearKeyCdmProxy : public CdmProxy {
 public:
  ClearKeyCdmProxy();
  ~ClearKeyCdmProxy() final;

  // CdmProxy implementation.
  void Initialize(Client* client, InitializeCB init_cb) final;
  void Process(Function function,
               uint32_t crypto_session_id,
               const std::vector<uint8_t>& input_data,
               uint32_t expected_output_data_size,
               ProcessCB process_cb) final;
  void CreateMediaCryptoSession(
      const std::vector<uint8_t>& input_data,
      CreateMediaCryptoSessionCB create_media_crypto_session_cb) final;
  void SetKey(uint32_t crypto_session_id,
              const std::vector<uint8_t>& key_id,
              const std::vector<uint8_t>& key_blob) final;
  void RemoveKey(uint32_t crypto_session_id,
                 const std::vector<uint8_t>& key_id) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClearKeyCdmProxy);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_CLEAR_KEY_CDM_CLEAR_KEY_CDM_PROXY_H_
