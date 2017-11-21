// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CDM_CONTEXT_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CDM_CONTEXT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/android/android_util.h"
#include "media/base/cdm_context.h"
#include "media/base/media_export.h"
#include "media/base/player_tracker.h"

namespace media {

// The CdmContext implementation for MediaDrmBridge. MediaDrmBridge supports
// neither Decryptor nor CDM ID, but uses MediaCrypto to connect to MediaCodec.
// MediaCodec-based decoders should cast the given CdmContext to this class to
// access APIs defined in this class.
//
// Methods can be called on any thread. The registered callbacks will be fired
// on any thread. The caller should make sure that the callbacks are posted to
// the correct thread.
//
// TODO(xhwang): Remove PlayerTracker interface.
class MEDIA_EXPORT MediaDrmBridgeCdmContext : public CdmContext,
                                              public PlayerTracker {
 public:
  // Notification called when MediaCrypto object is ready.
  // Parameters:
  // |media_crypto| - global reference to MediaCrypto object. |media_crypto| is
  //                  always a non-null std::unique_ptr, but the JavaRef it
  //                  contains can point to a null object.
  // |requires_secure_video_codec| - true if secure video decoder is required.
  //                                 Should be ignored if |media_crypto|
  //                                 contains null MediaCrypto object.
  using MediaCryptoReadyCB =
      base::Callback<void(JavaObjectPtr media_crypto,
                          bool requires_secure_video_codec)>;

  MediaDrmBridgeCdmContext() {}
  ~MediaDrmBridgeCdmContext() override {}

  virtual void SetMediaCryptoReadyCB(
      const MediaCryptoReadyCB& media_crypto_ready_cb) = 0;

  DISALLOW_COPY_AND_ASSIGN(MediaDrmBridgeCdmContext);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CDM_CONTEXT_H_
