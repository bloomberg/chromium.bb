// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_PROXY_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_PROXY_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "media/cdm/cdm_proxy.h"
#include "media/mojo/interfaces/cdm_proxy.mojom.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

class MojoCdmServiceContext;

// A mojom::CdmProxy implementation backed by a media::CdmProxy.
class MEDIA_MOJO_EXPORT MojoCdmProxyService : public mojom::CdmProxy,
                                              public CdmProxy::Client {
 public:
  MojoCdmProxyService(std::unique_ptr<::media::CdmProxy> cdm_proxy,
                      MojoCdmServiceContext* context);

  ~MojoCdmProxyService() final;

  // mojom::CdmProxy implementation.
  void Initialize(mojom::CdmProxyClientAssociatedPtrInfo client,
                  InitializeCallback callback) final;
  void Process(media::CdmProxy::Function function,
               uint32_t crypto_session_id,
               const std::vector<uint8_t>& input_data,
               uint32_t expected_output_data_size,
               ProcessCallback callback) final;
  void CreateMediaCryptoSession(
      const std::vector<uint8_t>& input_data,
      CreateMediaCryptoSessionCallback callback) final;
  void SetKey(uint32_t crypto_session_id,
              const std::vector<uint8_t>& key_id,
              const std::vector<uint8_t>& key_blob) final;
  void RemoveKey(uint32_t crypto_session_id,
                 const std::vector<uint8_t>& key_id) final;

  // CdmProxy::Client implementation.
  void NotifyHardwareReset() final;

 private:
  std::unique_ptr<::media::CdmProxy> cdm_proxy_;
  MojoCdmServiceContext* context_ = nullptr;

  mojom::CdmProxyClientAssociatedPtr client_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmProxyService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_PROXY_SERVICE_H_
