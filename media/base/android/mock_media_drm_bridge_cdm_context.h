// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MOCK_MEDIA_DRM_BRIDGE_CDM_CONTEXT_H_
#define MEDIA_BASE_ANDROID_MOCK_MEDIA_DRM_BRIDGE_CDM_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "media/base/android/media_drm_bridge_cdm_context.h"
#include "media/base/cdm_context.h"
#include "media/base/media_export.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MEDIA_EXPORT MockMediaDrmBridgeCdmContext
    : public CdmContext,
      public testing::NiceMock<MediaDrmBridgeCdmContext> {
 public:
  MockMediaDrmBridgeCdmContext();
  ~MockMediaDrmBridgeCdmContext() override;

  // CdmContext implementation.
  MediaDrmBridgeCdmContext* GetMediaDrmBridgeCdmContext() override;

  // MediaDrmBridgeCdmContext implementation.
  MOCK_METHOD2(RegisterPlayer,
               int(const base::Closure& new_key_cb,
                   const base::Closure& cdm_unset_cb));
  MOCK_METHOD1(UnregisterPlayer, void(int registration_id));
  MOCK_METHOD1(SetMediaCryptoReadyCB,
               void(const MediaCryptoReadyCB& media_crypto_ready_cb));

  static constexpr int kRegistrationId = 1000;

  base::Closure new_key_cb;
  base::Closure cdm_unset_cb;
  MediaCryptoReadyCB media_crypto_ready_cb;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaDrmBridgeCdmContext);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MOCK_MEDIA_DRM_BRIDGE_CDM_CONTEXT_H_
