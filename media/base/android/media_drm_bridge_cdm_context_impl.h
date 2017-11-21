// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CDM_CONTEXT_IMPL_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CDM_CONTEXT_IMPL_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "media/base/android/media_drm_bridge_cdm_context.h"
#include "media/base/media_export.h"

namespace media {

class MediaDrmBridge;

// Implementation of MediaDrmBridgeCdmContext.
//
// The registered callbacks will be fired on the thread |media_drm_bridge_| is
// running on.
class MEDIA_EXPORT MediaDrmBridgeCdmContextImpl
    : public MediaDrmBridgeCdmContext {
 public:
  // The |media_drm_bridge| owns |this| and is guaranteed to outlive |this|.
  explicit MediaDrmBridgeCdmContextImpl(MediaDrmBridge* media_drm_bridge);

  ~MediaDrmBridgeCdmContextImpl() final;

  // CdmContext implementation.
  Decryptor* GetDecryptor() final;
  int GetCdmId() const final;

  // PlayerTracker implementation.
  // Methods can be called on any thread. The registered callbacks will be fired
  // on |task_runner_|. The caller should make sure that the callbacks are
  // posted to the correct thread.
  //
  // Note: RegisterPlayer() must be called before SetMediaCryptoReadyCB() to
  // avoid missing any new key notifications.
  int RegisterPlayer(const base::Closure& new_key_cb,
                     const base::Closure& cdm_unset_cb) final;
  void UnregisterPlayer(int registration_id) final;

  // MediaDrmBridgeCdmContext implementation.
  void SetMediaCryptoReadyCB(
      const MediaCryptoReadyCB& media_crypto_ready_cb) final;

 private:
  MediaDrmBridge* const media_drm_bridge_;

  DISALLOW_COPY_AND_ASSIGN(MediaDrmBridgeCdmContextImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CDM_CONTEXT_IMPL_H_
